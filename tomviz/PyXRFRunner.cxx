/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PyXRFRunner.h"

#include "CameraReaction.h"
#include "LoadDataReaction.h"
#include "MainWindow.h"
#include "PyXRFDialog.h"
#include "PythonUtilities.h"
#include "Utilities.h"

#include "pipeline/Node.h"
#include "pipeline/Pipeline.h"
#include "pipeline/sources/PythonSource.h"

#include <QDebug>
#include <QMessageBox>
#include <QPointer>

namespace tomviz {

class PyXRFRunner::Internal : public QObject
{
public:
  QPointer<PyXRFRunner> parent;
  QPointer<QWidget> parentWidget;
  QPointer<PyXRFDialog> pyxrfDialog;

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

  template <typename T>
  void clearWidget(QPointer<T> d)
  {
    if (!d) {
      return;
    }

    d->hide();
    d->deleteLater();
    d.clear();
  }

  void clear()
  {
    clearWidget(pyxrfDialog);
  }

  void start()
  {
    clear();
    showPyXRFDialog();
  }

  void showPyXRFDialog()
  {
    clearWidget(pyxrfDialog);

    pyxrfDialog = new PyXRFDialog(parentWidget);
    connect(pyxrfDialog.data(), &QDialog::accepted, this,
            &Internal::pyxrfDialogAccepted);
    pyxrfDialog->show();
  }

  void pyxrfDialogAccepted()
  {
    auto* mainWindow = MainWindow::instance();
    auto* pip = mainWindow ? mainWindow->pipeline() : nullptr;
    if (!pip) {
      return;
    }

    auto* source = new pipeline::PythonSource();
    source->setJSONDescription(readInJSONDescription("PyXRFSource"));
    source->setScript(readInPythonScript("PyXRFSource"));

    source->setParameter("pyxrf_utils_command", pyxrfDialog->command());
    source->setParameter("working_directory",
                         pyxrfDialog->workingDirectory());
    source->setParameter("scan_range", pyxrfDialog->scanRange());
    source->setParameter("skip_scan_ids", pyxrfDialog->skipScanIds());
    source->setParameter("redownload_successful",
                         pyxrfDialog->redownloadSuccessful());
    source->setParameter("parameters_file", pyxrfDialog->parametersFile());
    source->setParameter("ic_name", pyxrfDialog->icName());
    source->setParameter("skip_processed", pyxrfDialog->skipProcessed());
    source->setParameter("rotate_datasets", pyxrfDialog->rotateDatasets());
    source->setParameter("csv_output", pyxrfDialog->csvOutput());

    LoadDataReaction::addSourceToPipeline(source);
    if (autoLoadFinalData) {
      LoadDataReaction::completeSourceSetup(source);
    }

    CameraReaction::resetPositiveZ();
    CameraReaction::rotateCamera(-90);

    source->markStale();
    pip->execute();
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
