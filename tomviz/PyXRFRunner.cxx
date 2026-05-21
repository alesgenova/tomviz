/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PyXRFRunner.h"

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

class PyXRFRunner::Internal : public QObject
{
public:
  QPointer<PyXRFRunner> parent;
  QPointer<QWidget> parentWidget;

  Python::Module pyxrfModule;

  bool autoLoadFinalData = true;

  Internal(PyXRFRunner* p) : parent(p)
  {
    setParent(p);
    parentWidget = mainWidget();
  }

  void importModule()
  {
    Python python;

    if (pyxrfModule.isValid()) {
      return;
    }

    pyxrfModule = python.import("tomviz.pyxrf");
    if (!pyxrfModule.isValid()) {
      qCritical() << "Failed to import \"tomviz.pyxrf\" module";
    }
  }

  bool isInstalled()
  {
    importModule();

    Python python;

    auto installed = pyxrfModule.findFunction("installed");
    if (!installed.isValid()) {
      qCritical() << "Failed to import \"tomviz.pyxrf.installed\"";
      return false;
    }

    auto res = installed.call();

    if (!res.isValid()) {
      qCritical() << "Error calling \"tomviz.pyxrf.installed\"";
      return false;
    }

    return res.toBool();
  }

  QString importError()
  {
    importModule();

    Python python;

    auto func = pyxrfModule.findFunction("import_error");
    if (!func.isValid()) {
      qCritical() << "Failed to import \"tomviz.pyxrf.import_error\"";
      return "import_error not found";
    }

    auto res = func.call();

    if (!res.isValid()) {
      qCritical() << "Error calling \"tomviz.pyxrf.import_error\"";
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
    source->setJSONDescription(readInJSONDescription("PyXRFSource"));
    source->setScript(readInPythonScript("PyXRFSource"));

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

PyXRFRunner::PyXRFRunner(QObject* parent)
  : QObject(parent), m_internal(new Internal(this))
{
}

PyXRFRunner::~PyXRFRunner() = default;

bool PyXRFRunner::isInstalled()
{
  return m_internal->isInstalled();
}

QString PyXRFRunner::importError()
{
  return m_internal->importError();
}

void PyXRFRunner::start()
{
  m_internal->start();
}

void PyXRFRunner::setAutoLoadFinalData(bool b)
{
  m_internal->autoLoadFinalData = b;
}

} // namespace tomviz
