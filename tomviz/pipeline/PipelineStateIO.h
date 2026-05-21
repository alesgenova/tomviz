/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePipelineStateIO_h
#define tomvizPipelinePipelineStateIO_h

#include <QJsonObject>
#include <QMap>

#include <functional>

class vtkSMViewProxy;

namespace tomviz {
namespace pipeline {

class Pipeline;

/// Save / load the new-format Tomviz state-file schema (schemaVersion >= 2).
/// Mirrors LegacyStateLoader on the legacy side: this handles only the new
/// graph-native format. The legacy loader stays in service for files that
/// lack schemaVersion.
///
/// The JSON payload has the shape:
///   {
///     "schemaVersion": 2,
///     "pipeline": {
///       "nextNodeId": <int>,
///       "nodes": [ { id, type, label, ... , config, outputPorts, inputPorts } ],
///       "links": [ { from: {node,port}, to: {node,port} } ]
///     },
///     "views":   [ ... ],   (added later)
///     "layouts": [ ... ],   (added later)
///     "paletteColor": [ r, g, b ]  (added later)
///   }
class PipelineStateIO
{
public:
  /// Serialize @a pipeline and (eventually) the surrounding view/layout
  /// state into @a outState. Only the "schemaVersion" and "pipeline"
  /// sections are populated; views/layouts/palette are appended by the
  /// caller via ViewsLayoutsSerializer for now.
  static bool save(Pipeline* pipeline, QJsonObject& outState);

  /// Populate @a pipeline from @a state produced by save(). The pipeline
  /// must be cleared by the caller beforehand. Returns false on
  /// unrecoverable parse errors; individual unknown node types / bad
  /// links are skipped with a warning.
  /// @a viewIdMap — if views have already been restored (typically via
  /// LegacyStateLoader::restoreViewsLayoutsAndPalette), pass the legacy
  /// view-id → proxy map here so each LegacyModuleSink can be bound to
  /// its saved view.
  /// @a preExecuteHook — invoked after nodes and links are built but
  /// before source nodes are eagerly executed. Containers that embed
  /// source payloads (e.g. Tvh5Format with HDF5 voxel groups) use this
  /// to populate source output ports directly, so the eager-execute
  /// pass becomes a no-op for those sources and the originals don't
  /// get re-read from disk.
  using PreExecuteHook =
    std::function<void(Pipeline*, const QJsonObject& pipelineJson)>;
  static bool load(
    Pipeline* pipeline, const QJsonObject& state,
    const QMap<int, vtkSMViewProxy*>& viewIdMap = {},
    const PreExecuteHook& preExecuteHook = {});
};

} // namespace pipeline
} // namespace tomviz

#endif
