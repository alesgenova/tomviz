/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineThresholdTransform_h
#define tomvizPipelineThresholdTransform_h

#include "TransformNode.h"

namespace tomviz {
namespace pipeline {

/// Transform that produces a binary mask volume: voxels with values in
/// [minValue, maxValue] are set to 1.0, all others to 0.0.
class ThresholdTransform : public TransformNode
{
  Q_OBJECT

public:
  ThresholdTransform(QObject* parent = nullptr);
  ~ThresholdTransform() override = default;

  void setMinValue(double min);
  void setMaxValue(double max);

  double minValue() const;
  double maxValue() const;

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override;

private:
  double m_minValue = -1e30;
  double m_maxValue = 0.0;
};

} // namespace pipeline
} // namespace tomviz

#endif
