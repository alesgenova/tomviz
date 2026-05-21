/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "VolumeStatsSink.h"

#include "data/VolumeData.h"

#include <vtkDataArray.h>

namespace tomviz {
namespace pipeline {

VolumeStatsSink::VolumeStatsSink(QObject* parent) : SinkNode(parent)
{
  addInput("volume", PortType::ImageData);
  setLabel("Volume Stats");
}

QIcon VolumeStatsSink::icon() const
{
  return QIcon(QStringLiteral(":/pqWidgets/Icons/pqSpreadsheet.svg"));
}

double VolumeStatsSink::min() const
{
  return m_min;
}

double VolumeStatsSink::max() const
{
  return m_max;
}

double VolumeStatsSink::mean() const
{
  return m_mean;
}

int VolumeStatsSink::voxelCount() const
{
  return m_voxelCount;
}

bool VolumeStatsSink::hasResults() const
{
  return m_hasResults;
}

bool VolumeStatsSink::consume(const QMap<QString, PortData>& inputs)
{
  auto inputVolume = inputs["volume"].value<VolumeDataPtr>();
  if (!inputVolume || !inputVolume->isValid()) {
    return false;
  }

  auto* scalars = inputVolume->scalars();
  if (!scalars || scalars->GetNumberOfTuples() == 0) {
    return false;
  }

  vtkIdType n = scalars->GetNumberOfTuples();
  m_voxelCount = static_cast<int>(n);

  double range[2];
  scalars->GetRange(range);
  m_min = range[0];
  m_max = range[1];

  double sum = 0.0;
  for (vtkIdType i = 0; i < n; ++i) {
    sum += scalars->GetTuple1(i);
  }
  m_mean = sum / n;

  m_hasResults = true;
  return true;
}

} // namespace pipeline
} // namespace tomviz
