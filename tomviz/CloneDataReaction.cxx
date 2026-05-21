/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "CloneDataReaction.h"

#include "ActiveObjects.h"
#include "LoadDataReaction.h"
#include "MainWindow.h"
#include "Utilities.h"

#include "pipeline/Pipeline.h"
#include "pipeline/PortData.h"
#include "pipeline/PortType.h"
#include "pipeline/PortUtils.h"
#include "pipeline/SourceNode.h"
#include "pipeline/data/VolumeData.h"

#include <vtkImageData.h>
#include <vtkNew.h>

#include <QApplication>
#include <QInputDialog>

namespace tomviz {

CloneDataReaction::CloneDataReaction(QAction* parentObject)
  : Reaction(parentObject)
{
}

DataSource* CloneDataReaction::clone(DataSource* toClone)
{
  // TODO: This still uses DataSource for the clone source. Once the pipeline
  // migration is complete, this should work entirely with SourceNode.
  // For now, we get the active SourceNode's VolumeData and deep-copy it.
  Q_UNUSED(toClone);

  // Get the VolumeData from the active pipeline (via MainWindow)
  auto* mainWindow = MainWindow::instance();
  if (!mainWindow) {
    return nullptr;
  }

  auto* pip = mainWindow->pipeline();
  if (!pip) {
    return nullptr;
  }

  // Find a source node to clone
  pipeline::SourceNode* activeSource = nullptr;
  for (auto* node : pip->nodes()) {
    auto* src = qobject_cast<pipeline::SourceNode*>(node);
    if (src) {
      activeSource = src;
      break;
    }
  }

  if (!activeSource) {
    return nullptr;
  }

  auto vol =
    pipeline::getOutputData<pipeline::VolumeDataPtr>(activeSource);
  if (!vol || !vol->imageData()) {
    return nullptr;
  }

  QStringList items;
  items << "Data only"
        << "Data with transformations";

  bool userOkayed;
  QString selection =
    QInputDialog::getItem(tomviz::mainWidget(), "Clone Data Options",
                          "Select what should be cloned", items,
                          /*current=*/0,
                          /*editable=*/false,
                          /*ok*/ &userOkayed);

  if (userOkayed) {
    // Deep-copy the vtkImageData
    vtkNew<vtkImageData> clonedImage;
    clonedImage->DeepCopy(vol->imageData());

    auto* newSource = new pipeline::SourceNode();
    newSource->setLabel(activeSource->label() + " (clone)");
    newSource->addOutput("volume", pipeline::PortType::ImageData);
    auto newVol = std::make_shared<pipeline::VolumeData>(clonedImage);
    newVol->setLabel(newSource->label());
    newSource->setOutputData(
      "volume",
      pipeline::PortData(newVol, pipeline::PortType::ImageData));

    LoadDataReaction::sourceNodeAdded(newSource);

    // TODO: operator cloning not yet supported in new pipeline
    return nullptr;
  }
  return nullptr;
}
} // namespace tomviz
