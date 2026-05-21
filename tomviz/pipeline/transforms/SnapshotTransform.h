/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineSnapshotTransform_h
#define tomvizPipelineSnapshotTransform_h

#include "TransformNode.h"
#include "data/VolumeData.h"

namespace tomviz {
namespace pipeline {

/// Transform that captures a deep copy of the input on first execution.
/// Subsequent executions return the cached copy unchanged.
/// Output is persistent (not transient).
class SnapshotTransform : public TransformNode
{
  Q_OBJECT

public:
  SnapshotTransform(QObject* parent = nullptr);
  ~SnapshotTransform() override = default;

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override;

private:
  VolumeDataPtr m_cachedSnapshot;
};

} // namespace pipeline
} // namespace tomviz

#endif
