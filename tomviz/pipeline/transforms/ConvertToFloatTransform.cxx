/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ConvertToFloatTransform.h"

#include "data/VolumeData.h"

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>

namespace {

template <typename T>
void convertToFloat(vtkFloatArray* fArray, int nComps, vtkIdType nTuples,
                    void* data)
{
  auto d = static_cast<T*>(data);
  auto a = static_cast<float*>(fArray->GetVoidPointer(0));
  for (vtkIdType i = 0; i < nComps * nTuples; ++i) {
    a[i] = static_cast<float>(d[i]);
  }
}

} // namespace

namespace tomviz {
namespace pipeline {

ConvertToFloatTransform::ConvertToFloatTransform(QObject* parent)
  : TransformNode(parent)
{
  addInput("volume", PortType::ImageData);
  addOutput("output", PortType::ImageData);
  setLabel("Convert to Float");
}

QMap<QString, PortData> ConvertToFloatTransform::transform(
  const QMap<QString, PortData>& inputs)
{
  QMap<QString, PortData> result;

  auto inputVolume = inputs["volume"].value<VolumeDataPtr>();
  if (!inputVolume || !inputVolume->isValid()) {
    return result;
  }

  auto* inputImage = inputVolume->imageData();
  auto* scalars = inputImage->GetPointData()->GetScalars();
  if (!scalars) {
    return result;
  }

  vtkNew<vtkFloatArray> floatArray;
  floatArray->SetNumberOfComponents(scalars->GetNumberOfComponents());
  floatArray->SetNumberOfTuples(scalars->GetNumberOfTuples());
  floatArray->SetName(scalars->GetName());

  switch (scalars->GetDataType()) {
    vtkTemplateMacro(convertToFloat<VTK_TT>(
      floatArray.Get(), scalars->GetNumberOfComponents(),
      scalars->GetNumberOfTuples(), scalars->GetVoidPointer(0)));
  }

  vtkNew<vtkImageData> outputImage;
  outputImage->DeepCopy(inputImage);
  outputImage->GetPointData()->RemoveArray(scalars->GetName());
  outputImage->GetPointData()->SetScalars(floatArray);

  auto volume = std::make_shared<VolumeData>(outputImage.Get());
  volume->setLabel(inputVolume->label());
  volume->setUnits(inputVolume->units());
  volume->setTiltAngles(inputVolume->tiltAngles());

  result["output"] = PortData(std::any(volume), inputs["volume"].type());
  return result;
}

} // namespace pipeline
} // namespace tomviz
