/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ReconstructionTransform.h"

#include "data/VolumeData.h"
#include "InputPort.h"
#include "ReconstructionWidget.h"
#include "TomographyReconstruction.h"
#include "TomographyTiltSeries.h"

#include <vtkDataArray.h>
#include <vtkFieldData.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>

#include <QVector>
#include <QtDebug>

namespace tomviz {
namespace pipeline {

ReconstructionTransform::ReconstructionTransform(QObject* parent)
  : TransformNode(parent)
{
  qRegisterMetaType<std::vector<float>>();
  addInput("tiltSeries", PortType::TiltSeries);
  addOutput("reconstruction", PortType::Volume);
  setLabel("Reconstruction");
  setSupportsCancel(true);
}

QIcon ReconstructionTransform::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqExtractGrid.svg");
}

QWidget* ReconstructionTransform::getCustomProgressWidget(
  QWidget* parent) const
{
  vtkImageData* inputImage = nullptr;
  vtkSMProxy* colorMap = nullptr;

  auto* input = inputPort("tiltSeries");
  if (input && input->hasData()) {
    auto portData = input->data();
    if (!portData.isValid() || !isVolumeType(portData.type())) {
      return nullptr;
    }
    auto vol = portData.value<VolumeDataPtr>();
    if (vol && vol->isValid()) {
      inputImage = vol->imageData();
      vol->initColorMap();
      colorMap = vol->colorMap();
    }
  }

  if (!inputImage) {
    return nullptr;
  }

  auto* widget = new ReconstructionWidget(inputImage, colorMap, parent);
  connect(this, &ReconstructionTransform::intermediateResults, widget,
          &ReconstructionWidget::updateIntermediateResults);
  connect(this, &Node::progressStepChanged, widget,
          &ReconstructionWidget::updateProgress);
  return widget;
}

QMap<QString, PortData> ReconstructionTransform::transform(
  const QMap<QString, PortData>& inputs)
{
  QMap<QString, PortData> result;

  auto inputVolume = inputs["tiltSeries"].value<VolumeDataPtr>();
  if (!inputVolume || !inputVolume->isValid()) {
    return result;
  }

  auto* imageData = inputVolume->imageData();
  int dataExtent[6];
  imageData->GetExtent(dataExtent);

  int numXSlices = dataExtent[1] - dataExtent[0] + 1;
  int numYSlices = dataExtent[3] - dataExtent[2] + 1;
  int numZSlices = dataExtent[5] - dataExtent[4] + 1;

  // Get tilt angles from VolumeData
  QVector<double> tiltAngles = inputVolume->tiltAngles();

  // Fallback: try the field data array
  if (tiltAngles.size() < numZSlices) {
    vtkFieldData* fd = imageData->GetFieldData();
    vtkDataArray* tiltAnglesArray = fd ? fd->GetArray("tilt_angles") : nullptr;
    if (tiltAnglesArray) {
      tiltAngles.resize(tiltAnglesArray->GetNumberOfTuples());
      for (int i = 0; i < tiltAngles.size(); ++i) {
        tiltAngles[i] = tiltAnglesArray->GetTuple1(i);
      }
    }
  }

  if (tiltAngles.size() < numZSlices) {
    qDebug() << "Incorrect number of tilt angles. There are"
             << tiltAngles.size() << "and there should be" << numZSlices;
    return result;
  }

  std::vector<float> sinogramPtr(numYSlices * numZSlices);
  std::vector<float> reconstructionPtr(numYSlices * numYSlices);

  vtkNew<vtkImageData> reconstructionImage;
  int extent2[6] = { dataExtent[0], dataExtent[1],  dataExtent[2],
                     dataExtent[3], dataExtent[2], dataExtent[3] };
  reconstructionImage->SetExtent(extent2);
  reconstructionImage->AllocateScalars(VTK_FLOAT, 1);
  vtkDataArray* darray = reconstructionImage->GetPointData()->GetScalars();
  darray->SetName("scalars");

  setTotalProgressSteps(numXSlices);
  float* reconstruction = static_cast<float*>(darray->GetVoidPointer(0));
  for (int i = 0; i < numXSlices && !isCanceled(); ++i) {
    TomographyTiltSeries::getSinogram(imageData, i, &sinogramPtr[0]);
    TomographyReconstruction::unweightedBackProjection2(
      &sinogramPtr[0], tiltAngles.data(), &reconstructionPtr[0], numZSlices,
      numYSlices);
    for (int j = 0; j < numYSlices; ++j) {
      for (int k = 0; k < numYSlices; ++k) {
        reconstruction[j * (numYSlices * numXSlices) + k * numXSlices + i] =
          reconstructionPtr[k * numYSlices + j];
      }
    }
    setProgressStep(i);
    emit intermediateResults(reconstructionPtr);
  }

  if (isCanceled()) {
    return result;
  }

  auto volume = std::make_shared<VolumeData>(reconstructionImage.Get());
  volume->setLabel("Reconstruction");
  volume->setUnits(inputVolume->units());

  result["reconstruction"] = PortData(std::any(volume), PortType::Volume);
  return result;
}

} // namespace pipeline
} // namespace tomviz
