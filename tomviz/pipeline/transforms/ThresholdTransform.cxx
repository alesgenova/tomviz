/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ThresholdTransform.h"

#include "data/VolumeData.h"

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>

namespace tomviz {
namespace pipeline {

ThresholdTransform::ThresholdTransform(QObject* parent) : TransformNode(parent)
{
  addInput("volume", PortType::ImageData);
  addOutput("mask", PortType::ImageData);
  setLabel("Threshold");
}

void ThresholdTransform::setMinValue(double min)
{
  m_minValue = min;
}

void ThresholdTransform::setMaxValue(double max)
{
  m_maxValue = max;
}

double ThresholdTransform::minValue() const
{
  return m_minValue;
}

double ThresholdTransform::maxValue() const
{
  return m_maxValue;
}

QMap<QString, PortData> ThresholdTransform::transform(
  const QMap<QString, PortData>& inputs)
{
  QMap<QString, PortData> result;

  auto inputVolume = inputs["volume"].value<VolumeDataPtr>();
  if (!inputVolume || !inputVolume->isValid()) {
    return result;
  }

  auto* inputImage = inputVolume->imageData();
  auto* inputScalars = inputImage->GetPointData()->GetScalars();
  if (!inputScalars) {
    return result;
  }

  int dims[3];
  inputImage->GetDimensions(dims);

  vtkNew<vtkImageData> outputImage;
  outputImage->SetDimensions(dims);
  outputImage->SetSpacing(inputImage->GetSpacing());
  outputImage->SetOrigin(inputImage->GetOrigin());

  vtkIdType numTuples = inputScalars->GetNumberOfTuples();
  vtkNew<vtkFloatArray> mask;
  mask->SetName("Mask");
  mask->SetNumberOfTuples(numTuples);

  for (vtkIdType i = 0; i < numTuples; ++i) {
    double val = inputScalars->GetTuple1(i);
    mask->SetValue(i, (val >= m_minValue && val <= m_maxValue) ? 1.0f : 0.0f);
  }

  outputImage->GetPointData()->SetScalars(mask);

  auto volume = std::make_shared<VolumeData>(outputImage.Get());
  volume->setLabel("Threshold Mask");
  volume->setUnits(inputVolume->units());

  result["mask"] = PortData(std::any(volume), PortType::ImageData);
  return result;
}

} // namespace pipeline
} // namespace tomviz
