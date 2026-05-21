/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataBrokerLoadReaction.h"
#include "DataBrokerLoadDialog.h"

#include "GenericHDF5Format.h"
#include "LoadDataReaction.h"
#include "Utilities.h"

#include "pipeline/PortData.h"
#include "pipeline/PortType.h"
#include "pipeline/SourceNode.h"
#include "pipeline/data/VolumeData.h"

#include <vtkImageData.h>

#include <QMessageBox>

namespace tomviz {

DataBrokerLoadReaction::DataBrokerLoadReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
}

DataBrokerLoadReaction::~DataBrokerLoadReaction() = default;

void DataBrokerLoadReaction::onTriggered()
{
  loadData();
}

void DataBrokerLoadReaction::loadData()
{
  auto dataBroker = new DataBroker(tomviz::mainWidget());
  DataBrokerLoadDialog dialog(dataBroker, tomviz::mainWidget());

  if (dialog.exec() == QDialog::Accepted) {
    auto catalog = dialog.selectedCatalog();
    auto runUid = dialog.selectedRunUid();
    auto table = dialog.selectedTable();
    auto variable = dialog.selectedVariable();

    tomviz::mainWidget()->setCursor(Qt::WaitCursor);
    auto call = dataBroker->loadVariable(catalog, runUid, table, variable);
    connect(call, &LoadDataCall::complete, dataBroker,
            [dataBroker, catalog, runUid, table,
             variable](vtkSmartPointer<vtkImageData> imageData) {
              // Relabel axes first, short-term workaround reorder to C
              // (again).
              GenericHDF5Format::reorderData(imageData,
                                             ReorderMode::FortranToC);
              relabelXAndZAxes(imageData);

              auto* source = new pipeline::SourceNode();
              auto label = QString("db:///%1/%2/%3/%4")
                             .arg(catalog)
                             .arg(runUid)
                             .arg(table)
                             .arg(variable);
              source->setLabel(label);
              source->addOutput("volume", pipeline::PortType::ImageData);
              auto volumeData =
                std::make_shared<pipeline::VolumeData>(imageData);
              volumeData->setLabel(label);
              source->setOutputData(
                "volume",
                pipeline::PortData(volumeData, pipeline::PortType::ImageData));
              source->setProperty("dataType", "tiltSeries");

              LoadDataReaction::sourceNodeAdded(source, true, false);
              dataBroker->deleteLater();
              tomviz::mainWidget()->unsetCursor();
            });

    connect(call, &DataBrokerCall::error, dataBroker,
            [dataBroker](const QString& errorMessage) {
              tomviz::mainWidget()->unsetCursor();
              dataBroker->deleteLater();
              QMessageBox messageBox(
                QMessageBox::Warning, "tomviz",
                QString("Error loading DataBroker dataset: %1. Please check "
                        "message log for details.")
                  .arg(errorMessage),
                QMessageBox::Ok);
              messageBox.exec();
            });
  }
}

} // namespace tomviz
