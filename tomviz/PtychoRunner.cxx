/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PtychoRunner.h"

#include "CameraReaction.h"
#include "LoadDataReaction.h"
#include "MainWindow.h"
#include "PythonUtilities.h"
#include "Utilities.h"

#include "pipeline/DeferredLinkInfo.h"
#include "pipeline/Node.h"
#include "pipeline/NodeEditDialog.h"
#include "pipeline/Pipeline.h"
#include "pipeline/SourceNode.h"
#include "pipeline/sources/PythonSource.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPointer>

namespace tomviz {

class PtychoRunner::Internal : public QObject
{
public:
  QPointer<PtychoRunner> parent;
  QPointer<QWidget> parentWidget;

  Python::Module ptychoModule;

  bool autoLoadFinalData = true;

  Internal(PtychoRunner* p) : parent(p)
  {
    setParent(p);
    parentWidget = mainWidget();
  }

  void importModule()
  {
    Python python;

    if (ptychoModule.isValid()) {
      return;
    }

    ptychoModule = python.import("tomviz.ptycho");
    if (!ptychoModule.isValid()) {
      qCritical() << "Failed to import \"tomviz.ptycho\" module";
    }
  }

  bool isInstalled()
  {
    importModule();

    Python python;

    auto installed = ptychoModule.findFunction("installed");
    if (!installed.isValid()) {
      qCritical() << "Failed to import \"tomviz.ptycho.installed\"";
      return false;
    }

    auto res = installed.call();

    if (!res.isValid()) {
      qCritical() << "Error calling \"tomviz.ptycho.installed\"";
      return false;
    }

    return res.toBool();
  }

  QString importError()
  {
    importModule();

    Python python;

    auto func = ptychoModule.findFunction("import_error");
    if (!func.isValid()) {
      qCritical() << "Failed to import \"tomviz.ptycho.import_error\"";
      return "import_error not found";
    }

    auto res = func.call();

    if (!res.isValid()) {
      qCritical() << "Error calling \"tomviz.ptycho.import_error\"";
      return "import_error not found";
    }

    return res.toString();
  }

  void start()
  {
    auto* mainWindow = MainWindow::instance();
    auto* pip = mainWindow ? mainWindow->pipeline() : nullptr;
    if (!pip) {
      return;
    }

    auto* source = new pipeline::PythonSource();
    source->setJSONDescription(readInJSONDescription("PtychoSource"));
    source->setScript(readInPythonScript("PtychoSource"));

    LoadDataReaction::addSourceToPipeline(source);

    pipeline::DeferredLinkInfo deferred;
    auto* dialog =
      new pipeline::NodeEditDialog(source, pip, deferred, mainWindow);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QString title = QJsonDocument::fromJson(
      source->jsonDescription().toUtf8())
        .object()
        .value(QStringLiteral("label"))
        .toString();
    if (title.isEmpty()) {
      title = source->label();
    }
    dialog->setWindowTitle(QString("Generate - %1").arg(title));

    QObject::connect(
      dialog, &pipeline::NodeEditDialog::insertionCompleted, dialog,
      [this](pipeline::Node* node) {
        if (auto* src = qobject_cast<pipeline::SourceNode*>(node)) {
          if (autoLoadFinalData) {
            LoadDataReaction::completeSourceSetup(src);
          }
        }
        CameraReaction::resetPositiveZ();
        CameraReaction::rotateCamera(-90);
      });

    dialog->show();
  }
};

PtychoRunner::PtychoRunner(QObject* parent)
  : QObject(parent), m_internal(new Internal(this))
{
}

PtychoRunner::~PtychoRunner() = default;

bool PtychoRunner::isInstalled()
{
  return m_internal->isInstalled();
}

QString PtychoRunner::importError()
{
  return m_internal->importError();
}

void PtychoRunner::start()
{
  m_internal->start();
}

void PtychoRunner::setAutoLoadFinalData(bool b)
{
  m_internal->autoLoadFinalData = b;
}

} // namespace tomviz
