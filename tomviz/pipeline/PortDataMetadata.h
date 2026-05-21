/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePortDataMetadata_h
#define tomvizPipelinePortDataMetadata_h

#include "PortData.h"

#include <QMap>
#include <QString>

class QObject;

namespace tomviz {
namespace pipeline {

class VolumeData;

/// Build and apply a fresh segmentation-style colormap to @a vol based
/// on its current active scalars. Used for LabelMap outputs, where the
/// label set can vary between executions and inheriting from upstream
/// would yield wrong colors.
///
/// Returns true if a segmentation preset was applied.
bool applySegmentationColorMap(VolumeData& vol);

/// Copy presentation metadata (e.g. colormap, gradient opacity) from
/// each input PortData onto the matching output PortData whenever the
/// payload kind supports it and the output doesn't already carry its
/// own. Called by nodes between produce-output and publish-output so
/// downstream consumers see the inherited state from their first
/// frame.
///
/// Per-type dispatch lives entirely in this function — callers stay
/// payload-agnostic. Today: volume-typed payloads inherit colormap +
/// gradient opacity from their first volume-typed input. Adding a
/// new inheritable type means a new branch here.
///
/// @a threadOwner is the QObject whose thread should run the
/// inheritance work. ParaView SM-proxy operations underneath VolumeData
/// aren't thread-safe; this function marshals to threadOwner's thread
/// via Qt::BlockingQueuedConnection when the caller is elsewhere.
void inheritOutputMetadata(QObject* threadOwner,
                           const QMap<QString, PortData>& inputs,
                           const QMap<QString, PortData>& outputs);

} // namespace pipeline
} // namespace tomviz

#endif
