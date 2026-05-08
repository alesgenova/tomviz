/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PtychoRunner.h"

#include "ActiveObjects.h"
#include "CameraReaction.h"
#include "LoadDataReaction.h"
#include "MainWindow.h"
#include "PtychoDialog.h"
#include "PythonUtilities.h"
#include "Utilities.h"

#include "pipeline/Node.h"
#include "pipeline/Pipeline.h"
#include "pipeline/sources/PythonSource.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPointer>

namespace tomviz {

class PtychoRunner::Internal : public QObject
{
public:
  QPointer<PtychoRunner> parent;
  QPointer<QWidget> parentWidget;
  QPointer<PtychoDialog> ptychoDialog;

  // Python modules and functions (for isInstalled/importError only)
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
    clearWidget(ptychoDialog);
  }

  void start()
  {
    clear();
    showPtychoDialog();
  }

  void showPtychoDialog()
  {
    clearWidget(ptychoDialog);

    ptychoDialog = new PtychoDialog(parentWidget);
    connect(ptychoDialog.data(), &QDialog::accepted, this,
            &Internal::ptychoDialogAccepted);
    ptychoDialog->show();
  }

  void ptychoDialogAccepted()
  {
    auto* mainWindow = MainWindow::instance();
    auto* pip = mainWindow ? mainWindow->pipeline() : nullptr;
    if (!pip) {
      return;
    }

    // Gather parameters from the dialog
    QString ptychoDirectory = ptychoDialog->ptychoDirectory();
    QList<long> sidList = ptychoDialog->selectedSids();
    QStringList versionList = ptychoDialog->selectedVersions();
    QList<double> angleList = ptychoDialog->selectedAngles();
    QString outputInfoFile = ptychoDialog->outputInfoFile();
    bool rotateDatasets = ptychoDialog->rotateDatasets();

    // Build JSON-encoded parameter strings
    QJsonArray sidArray;
    for (auto sid : sidList) {
      sidArray.append(static_cast<qint64>(sid));
    }
    QJsonArray versionArray;
    for (const auto& v : versionList) {
      versionArray.append(v);
    }
    QJsonArray angleArray;
    for (auto angle : angleList) {
      angleArray.append(angle);
    }

    // Create the PythonSource node
    auto* source = new pipeline::PythonSource();
    source->setJSONDescription(readInJSONDescription("PtychoSource"));
    source->setScript(readInPythonScript("PtychoSource"));

    source->setParameter("ptycho_dir", ptychoDirectory);
    source->setParameter("output_info_file", outputInfoFile);
    source->setParameter("rotate_datasets", rotateDatasets);
    source->setParameter("sid_list",
      QString::fromUtf8(QJsonDocument(sidArray).toJson(
        QJsonDocument::Compact)));
    source->setParameter("version_list",
      QString::fromUtf8(QJsonDocument(versionArray).toJson(
        QJsonDocument::Compact)));
    source->setParameter("angle_list",
      QString::fromUtf8(QJsonDocument(angleArray).toJson(
        QJsonDocument::Compact)));

    // Add to pipeline and set up sinks/color map
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
