/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonSource.h"

#include "CustomNodeWidgetRegistry.h"
#include "CustomPythonNodeWidget.h"
#include "ExternalNodeExecutor.h"
#include "OutputPort.h"
#include "ParameterBindingUtils.h"
#include "PythonNodeEditorWidget.h"

namespace tomviz {
namespace pipeline {

PythonSource::PythonSource(QObject* parent) : SourceNode(parent)
{
}

void PythonSource::setJSONDescription(const QString& json)
{
  m_backend.setJSONDescription(json);
  if (!m_backend.defaultLabel().isEmpty()) {
    setLabel(m_backend.defaultLabel());
  }
  setSupportsCancel(m_backend.supportsCancel());
  setSupportsCompletion(m_backend.supportsComplete());
  // Shape mismatch (a transform-shaped description loaded by a source
  // shell) is rejected upstream by the menu-routing reaction; loading
  // a saved state file trusts its serialized type string. Either way,
  // a SourceNode never accepts inputs — silently ignore any inputs[]
  // entries the description still carries.
  m_backend.applyDescription(
    nullptr,
    [this](const QString& name, PortType type) {
      return addOutput(name, type);
    });
}

QString PythonSource::jsonDescription() const
{
  return m_backend.jsonDescription();
}

void PythonSource::setScript(const QString& script)
{
  m_backend.setScript(script);
}

QString PythonSource::scriptSource() const
{
  return m_backend.scriptSource();
}

void PythonSource::setParameter(const QString& name, const QVariant& value)
{
  m_backend.setParameter(name, value);
}

QVariant PythonSource::parameter(const QString& name) const
{
  return m_backend.parameter(name);
}

QMap<QString, QVariant> PythonSource::parameters() const
{
  return m_backend.parameters();
}

QString PythonSource::operatorName() const
{
  return m_backend.operatorName();
}

bool PythonSource::hasPropertiesWidget() const
{
  return true;
}

EditNodeWidget* PythonSource::createPropertiesWidget(Pipeline* pipeline,
                                                     QWidget* parent)
{
  PythonNodeEditorWidget::CustomWidgetFactory factory;
  bool customNeedsData = false;
  auto widgetID = m_backend.customWidgetID();
  const CustomWidgetInfo* info =
    widgetID.isEmpty() ? nullptr : findCustomNodeWidget(widgetID);
  if (info && info->create) {
    // Sources have no inputs — needsData should be irrelevant here,
    // but honor the flag in case a registration unexpectedly sets it.
    customNeedsData = info->needsData;
    factory = [this, info](QWidget* p) -> CustomPythonNodeWidget* {
      auto* w = info->create(collectInputs(), p);
      if (w) {
        w->setScript(m_backend.scriptSource());
      }
      return w;
    };
  }

  QString currentType;
  QString currentEnvPath;
  if (auto* ext = qobject_cast<ExternalNodeExecutor*>(nodeExecutor())) {
    currentType = ExternalNodeExecutor::typeString();
    currentEnvPath = ext->envPath();
  }

  auto* widget = new PythonNodeEditorWidget(
    this, pipeline, label(), m_backend.scriptSource(),
    m_backend.jsonDescription(), m_backend.parameters(), currentType,
    currentEnvPath, factory, customNeedsData, parent);

  connect(widget, &PythonNodeEditorWidget::applied, this,
          [this](const QString& newLabel,
                 const QString& newScript,
                 const QMap<QString, QVariant>& values,
                 const QString& executorType,
                 const QString& executorEnvPath) {
            bool changed = false;

            if (label() != newLabel) {
              setLabel(newLabel);
              changed = true;
            }

            if (m_backend.scriptSource() != newScript) {
              m_backend.setScript(newScript);
              changed = true;
            }

            for (auto it = values.constBegin();
                 it != values.constEnd(); ++it) {
              if (m_backend.parameter(it.key()) != it.value()) {
                changed = true;
              }
              m_backend.setParameter(it.key(), it.value());
            }

            auto* currentExternal =
              qobject_cast<ExternalNodeExecutor*>(nodeExecutor());
            if (executorType.isEmpty()) {
              if (nodeExecutor() != nullptr) {
                setNodeExecutor(nullptr);
                changed = true;
              }
            } else if (executorType == ExternalNodeExecutor::typeString()) {
              if (currentExternal) {
                if (currentExternal->envPath() != executorEnvPath) {
                  currentExternal->setEnvPath(executorEnvPath);
                  changed = true;
                }
              } else {
                setNodeExecutor(new ExternalNodeExecutor(executorEnvPath));
                changed = true;
              }
            }

            if (changed) {
              emit parametersApplied();
            }
          });

  wireParameterBindings(this, widget, m_backend.parameterBindings());

  return widget;
}

bool PythonSource::execute()
{
  resetExecutionFlags();
  resetProgress();
  setExecState(NodeExecState::Running);

  auto outputs = m_backend.runSource(this);

  if (isCanceled()) {
    setExecState(NodeExecState::Canceled);
    return false;
  }

  if (outputs.isEmpty() && !outputPorts().isEmpty()) {
    setExecState(NodeExecState::Failed);
    return false;
  }

  applyOutputs(outputs);

  markCurrent();
  setExecState(NodeExecState::Idle);
  return true;
}

QJsonObject PythonSource::serialize() const
{
  return m_backend.serializeInto(SourceNode::serialize());
}

bool PythonSource::deserialize(const QJsonObject& json)
{
  m_backend.applySerializedFields(
    json,
    nullptr,
    [this](const QString& name, PortType type) {
      return addOutput(name, type);
    });
  setSupportsCancel(m_backend.supportsCancel());
  setSupportsCompletion(m_backend.supportsComplete());
  if (!SourceNode::deserialize(json)) {
    return false;
  }
  if (!nodeExecutor()) {
    auto envPath = m_backend.externalPythonEnvPath();
    if (!envPath.isEmpty()) {
      setNodeExecutor(new ExternalNodeExecutor(envPath));
    }
  }
  return true;
}

} // namespace pipeline
} // namespace tomviz
