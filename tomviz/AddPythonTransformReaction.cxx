/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AddPythonTransformReaction.h"

#include "ActiveObjects.h"
#include "MainWindow.h"
#include "TransformUtils.h"

#include "pipeline/InputPort.h"
#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/TransformNode.h"
#include "pipeline/transforms/LegacyPythonTransform.h"
#include "pipeline/transforms/PythonTransform.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QtDebug>

namespace tomviz {

static pipeline::PortType portTypeFromString(const QString& str)
{
  if (str == "TiltSeries")
    return pipeline::PortType::TiltSeries;
  if (str == "Volume")
    return pipeline::PortType::Volume;
  if (str == "ImageData")
    return pipeline::PortType::ImageData;
  return pipeline::PortType::None;
}

namespace {

/// Returns true when the operator description declares schemaVersion 2.
/// Empty / non-object / missing schemaVersion all default to v1.
bool isSchemaV2(const QString& json)
{
  if (json.isEmpty()) {
    return false;
  }
  auto doc = QJsonDocument::fromJson(json.toUtf8());
  if (!doc.isObject()) {
    return false;
  }
  return doc.object().value(QStringLiteral("schemaVersion")).toInt(1) == 2;
}

template <typename T>
T* configurePythonTransform(T* node, const QString& label,
                            const QString& script, const QString& json,
                            const QMap<QString, QVariant>& arguments)
{
  node->setLabel(label);
  node->setScript(script);
  if (!json.isEmpty()) {
    node->setJSONDescription(json);
  }
  for (auto it = arguments.constBegin(); it != arguments.constEnd(); ++it) {
    node->setParameter(it.key(), it.value());
  }
  return node;
}

/// Build the right Python transform node for @a json (legacy if no
/// schemaVersion or v1, else PythonTransform). Returns nullptr if the
/// JSON declares schemaVersion 2 but has empty inputs — that's a
/// source-shape description being routed to a transform-only entry
/// point and is rejected at construction (Q4 design choice).
pipeline::TransformNode* makePythonTransform(
  const QString& label, const QString& script, const QString& json,
  const QMap<QString, QVariant>& arguments)
{
  if (isSchemaV2(json)) {
    auto doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.object().value(QStringLiteral("inputs")).toArray().isEmpty()) {
      qCritical()
        << "AddPythonTransformReaction: schema-v2 description for"
        << label
        << "declares no inputs (it is source-shaped) — refusing to "
           "create a transform from a source description.";
      return nullptr;
    }
    return configurePythonTransform(
      new pipeline::PythonTransform(), label, script, json, arguments);
  }
  return configurePythonTransform(
    new pipeline::LegacyPythonTransform(), label, script, json, arguments);
}

} // namespace

AddPythonTransformReaction::AddPythonTransformReaction(
  QAction* parentObject, const QString& l, const QString& s,
  const QString& json)
  : pqReaction(parentObject), jsonSource(json), scriptLabel(l), scriptSource(s),
    interactive(false)
{
  connect(&ActiveObjects::instance(),
          &ActiveObjects::activePipelineChanged,
          this, &AddPythonTransformReaction::updateEnableState);
  connect(&ActiveObjects::instance(),
          &ActiveObjects::activeTipOutputPortChanged,
          this, &AddPythonTransformReaction::updateEnableState);

  // Parse JSON to extract inputType and externalCompatible.
  if (!json.isEmpty()) {
    auto document = QJsonDocument::fromJson(json.toLatin1());
    if (!document.isObject()) {
      qCritical() << "Failed to parse operator JSON";
      qCritical() << json;
      return;
    }
    QJsonObject root = document.object();
    QJsonValueRef externalNode = root["externalCompatible"];
    if (!externalNode.isUndefined() && !externalNode.isNull()) {
      this->externalCompatible = externalNode.toBool();
    }
    m_isSchemaV2 =
      root.value(QStringLiteral("schemaVersion")).toInt(1) == 2;
    if (m_isSchemaV2) {
      // v2 derives accepted input types from the inputs[] array. If
      // empty, the description is source-shaped and shouldn't be
      // routed through this transform-flavored reaction at all —
      // disable the action permanently.
      auto inputs = root.value(QStringLiteral("inputs")).toArray();
      if (inputs.isEmpty()) {
        qCritical()
          << "AddPythonTransformReaction: schema-v2 description for"
          << l
          << "declares no inputs (it is source-shaped) — disabling.";
        m_disabled = true;
      } else {
        // Use the first input's declared type as the accepted gate.
        // Multi-input v2 transforms keep the same first-input gating
        // the legacy `inputType` field provided.
        auto firstType =
          inputs.first().toObject().value(QStringLiteral("type")).toString();
        auto pt = portTypeFromString(firstType);
        if (pt != pipeline::PortType::None) {
          m_acceptedInputTypes = pt;
        }
      }
    } else if (root.contains("inputType")) {
      auto pt = portTypeFromString(root.value("inputType").toString());
      if (pt != pipeline::PortType::None) {
        m_acceptedInputTypes = pt;
      }
    }
  }

  // Extract the description from JSON and set it on the action for the search
  // dialog to display.
  if (!json.isEmpty()) {
    auto descDoc = QJsonDocument::fromJson(json.toLatin1());
    if (descDoc.isObject()) {
      QString desc = descDoc.object().value("description").toString();
      if (!desc.isEmpty()) {
        parentObject->setStatusTip(desc);
      }
    }
  }

  qApp->installEventFilter(this);
  updateEnableState();
}

bool AddPythonTransformReaction::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::KeyPress ||
      event->type() == QEvent::KeyRelease) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Control) {
      m_ctrlHeld = (event->type() == QEvent::KeyPress);
      updateEnableState();
    }
  }
  return pqReaction::eventFilter(obj, event);
}

void AddPythonTransformReaction::updateEnableState()
{
  // Permanently disabled (e.g. malformed schema-v2 description).
  if (m_disabled) {
    parentAction()->setEnabled(false);
    return;
  }

  // Ctrl held: user wants to add an unconnected node, so always enable.
  if (m_ctrlHeld) {
    parentAction()->setEnabled(true);
    return;
  }

  auto& ao = ActiveObjects::instance();
  auto* tipPort = ao.activeTipOutputPort();
  if (!tipPort) {
    parentAction()->setEnabled(false);
    return;
  }
  parentAction()->setEnabled(
    pipeline::isPortTypeCompatible(tipPort->type(), m_acceptedInputTypes));
}

OperatorPython* AddPythonTransformReaction::addExpression(DataSource*)
{
  auto* mainWindow = MainWindow::instance();
  auto* pip = mainWindow ? mainWindow->pipeline() : nullptr;
  if (!pip) {
    return nullptr;
  }

  auto* transform =
    makePythonTransform(scriptLabel, scriptSource, jsonSource, {});
  if (!transform) {
    return nullptr;
  }
  insertTransformIntoPipeline(transform);
  return nullptr;
}

void AddPythonTransformReaction::addExpressionFromNonModalDialog()
{
  // Non-modal dialogs for Clear Volume and Background Subtraction are not
  // yet supported in the new pipeline. Log and return.
  qWarning("Non-modal transform dialogs not yet supported in new pipeline.");
}

void AddPythonTransformReaction::addPythonOperator(
  DataSource*, const QString& scriptLabel,
  const QString& scriptBaseString, const QMap<QString, QVariant> arguments,
  const QString& jsonString)
{
  auto* transform = makePythonTransform(
    scriptLabel, scriptBaseString, jsonString, arguments);
  if (!transform) {
    return;
  }
  insertTransformIntoPipeline(transform);
}

void AddPythonTransformReaction::addPythonOperator(
  DataSource*, const QString& scriptLabel,
  const QString& scriptBaseString, const QMap<QString, QVariant> arguments,
  const QMap<QString, QString>)
{
  // No JSON description provided → can't be schema-v2; legacy path.
  auto* transform = makePythonTransform(
    scriptLabel, scriptBaseString, /*json=*/QString(), arguments);
  if (!transform) {
    return;
  }
  insertTransformIntoPipeline(transform);
}
} // namespace tomviz
