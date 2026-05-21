/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineSphereSource_h
#define tomvizPipelineSphereSource_h

#include "SourceNode.h"

namespace tomviz {
namespace pipeline {

/// A source node that generates a small volume containing a sphere.
/// Voxel values represent the signed distance from the sphere surface
/// (negative inside, positive outside), making it suitable for
/// contouring or thresholding.
class SphereSource : public SourceNode
{
  Q_OBJECT

public:
  SphereSource(QObject* parent = nullptr);
  ~SphereSource() override = default;

  /// Set the volume dimensions (default 32x32x32)
  void setDimensions(int x, int y, int z);

  /// Set the sphere radius as a fraction of the smallest dimension (default 0.4)
  void setRadiusFraction(double fraction);

  /// Generate the volume data and set it on the output port
  bool execute() override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

private:
  int m_dimensions[3] = { 32, 32, 32 };
  double m_radiusFraction = 0.4;
};

} // namespace pipeline
} // namespace tomviz

#endif
