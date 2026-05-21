/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SnapshotTransform.h"

#include "data/VolumeData.h"

#include <vtkImageData.h>
#include <vtkNew.h>

namespace tomviz {
namespace pipeline {

SnapshotTransform::SnapshotTransform(QObject* parent) : TransformNode(parent)
{
  addInput("volume", PortType::ImageData);
  addOutput("snapshot", PortType::ImageData);
  setLabel("Snapshot");
}

QMap<QString, PortData> SnapshotTransform::transform(
  const QMap<QString, PortData>& inputs)
{
  QMap<QString, PortData> result;

  // If we already have a cached snapshot, return it unchanged
  if (m_cachedSnapshot) {
    result["snapshot"] =
      PortData(std::any(m_cachedSnapshot), PortType::ImageData);
    return result;
  }

  auto inputVolume = inputs["volume"].value<VolumeDataPtr>();
  if (!inputVolume || !inputVolume->isValid()) {
    return result;
  }

  // Deep copy the input image
  vtkNew<vtkImageData> cacheImage;
  cacheImage->DeepCopy(inputVolume->imageData());

  m_cachedSnapshot = std::make_shared<VolumeData>(cacheImage.Get());
  m_cachedSnapshot->setLabel("Snapshot");
  m_cachedSnapshot->setUnits(inputVolume->units());
  m_cachedSnapshot->setTiltAngles(inputVolume->tiltAngles());

  result["snapshot"] =
    PortData(std::any(m_cachedSnapshot), PortType::ImageData);
  return result;
}

} // namespace pipeline
} // namespace tomviz
