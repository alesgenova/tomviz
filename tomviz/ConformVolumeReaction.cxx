/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ConformVolumeReaction.h"

#include "vtkImageChangeInformation.h"
#include "vtkImageData.h"
#include "vtkImageResize.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"

#include "ConformVolumeDialog.h"
#include "legacy/DataSource.h"
#include "LoadDataReaction.h"

#include "pipeline/PortData.h"
#include "pipeline/PortType.h"
#include "pipeline/SourceNode.h"
#include "pipeline/data/VolumeData.h"

namespace tomviz {

ConformVolumeReaction::ConformVolumeReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  updateEnableState();
}

void ConformVolumeReaction::onTriggered()
{
  // Check which volume should be the conforming one
  ConformVolumeDialog dialog;
  dialog.setVolumes(m_dataSources.values());

  if (dialog.exec() == QDialog::Rejected) {
    return;
  }

  m_conformingVolume = dialog.selectedVolume();

  auto* newSource = createConformedVolume();
  if (newSource) {
    auto* source = new pipeline::SourceNode();
    source->setLabel("Conformed Volume");
    source->addOutput("volume", pipeline::PortType::ImageData);
    vtkSmartPointer<vtkImageData> img = newSource->imageData();
    auto vol = std::make_shared<pipeline::VolumeData>(img);
    vol->setLabel("Conformed Volume");
    source->setOutputData(
      "volume",
      pipeline::PortData(vol, pipeline::PortType::ImageData));
    LoadDataReaction::sourceNodeAdded(source);
    delete newSource; // Clean up the temporary DataSource
  }
}

void ConformVolumeReaction::updateDataSources(QSet<DataSource*> sources)
{
  m_dataSources = sources;
  updateEnableState();
}

void ConformVolumeReaction::updateEnableState()
{
  updateVisibleState();
}

void ConformVolumeReaction::updateVisibleState()
{
  parentAction()->setVisible(false);

  if (m_dataSources.size() != 2) {
    return;
  }

  QList<DataSource*> sourceList = m_dataSources.values();
  // Check that both DataSources are volumes
  for (auto ds : m_dataSources) {
    if (ds->type() != DataSource::Volume) {
      return;
    }
  }

  // Make sure the names are not the same...
  if (sourceList[0]->label() == sourceList[1]->label()) {
    return;
  }

  parentAction()->setVisible(true);
}

DataSource* ConformVolumeReaction::createConformedVolume()
{
  if (m_dataSources.size() != 2 ||
      !m_dataSources.contains(m_conformingVolume)) {
    return nullptr;
  }

  auto* conformingVolume = m_conformingVolume;

  DataSource* conformToVolume = nullptr;
  for (auto it = m_dataSources.cbegin(); it != m_dataSources.cend(); ++it) {
    if (*it != conformingVolume) {
      conformToVolume = *it;
      break;
    }
  }

  if (!conformToVolume) {
    return nullptr;
  }

  // Begin the volume conforming
  vtkNew<vtkImageResize> resize;
  resize->SetInputData(conformingVolume->imageData());
  resize->SetOutputDimensions(conformToVolume->imageData()->GetDimensions());
  resize->Update();

  vtkNew<vtkImageChangeInformation> changeInfo;
  changeInfo->SetInputData(resize->GetOutputDataObject(0));
  changeInfo->SetInformationInputData(conformToVolume->imageData());
  changeInfo->Update();

  auto* output = vtkImageData::SafeDownCast(changeInfo->GetOutputDataObject(0));
  if (!output) {
    return nullptr;
  }

  // TODO: Fully migrate ConformVolumeReaction to use SourceNode
  auto* newSource = new DataSource(output);
  newSource->setFileName("Conformed Volume");
  // Make the display position match as well
  newSource->setDisplayPosition(conformToVolume->displayPosition());
  return newSource;
}

} // namespace tomviz
