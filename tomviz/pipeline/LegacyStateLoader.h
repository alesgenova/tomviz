/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineLegacyStateLoader_h
#define tomvizPipelineLegacyStateLoader_h

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

class vtkImageData;
class vtkSMViewProxy;

namespace h5 {
class H5ReadWrite;
}

namespace tomviz {
namespace pipeline {

class Node;
class OutputPort;
class Pipeline;
class SourceNode;
class TransformNode;
class SinkGroupNode;
class LegacyModuleSink;

/// Load a legacy Tomviz state file (.tvsm JSON or .tvh5 HDF5) into the
/// current new-pipeline graph. Legacy state produced by older Tomviz
/// versions still rides on the same JSON schema; the loader walks that
/// schema and creates the equivalent SourceNode / TransformNode / SinkNode
/// tree.
///
/// This is a one-way compatibility layer — it is not a saver. Saving and
/// the new state-file format come in later stages.
class LegacyStateLoader
{
public:
  /// Load from a parsed .tvsm JSON object.  @a stateDir is used to resolve
  /// relative reader file paths stored in each DataSource's "reader"
  /// section.  When @a executePipeline is false, the pipeline graph is
  /// built and the view/sink state is applied but operator execution is
  /// skipped — the user will run the pipeline manually later.
  /// Returns false on any unrecoverable error (individual unrecognized
  /// operators/modules are skipped, not fatal).
  static bool load(const QJsonObject& state, const QDir& stateDir,
                   bool executePipeline = true);

  /// Load from a .tvh5 file on disk.  Opens the HDF5 container, extracts
  /// the /tomviz_state JSON, then walks the same structure as load() but
  /// with data arrays coming from the HDF5 groups rather than from
  /// separate files on disk.
  static bool loadFromH5(const QString& filename,
                         bool executePipeline = true);

  /// Load a v1 template: walk @a operators (an "operators" array from a
  /// legacy .tvsm file), build each as a TransformNode, and chain them
  /// linearly via createLink on the active pipeline. Any nested child
  /// dataSources / modules on the operators are intentionally ignored —
  /// templates are transform-only fragments.
  static void loadTemplateOperators(const QJsonArray& operators,
                                    Pipeline* pipeline);

  /// Restore the top-level `views`, `layouts`, and `paletteColor`
  /// sections of @a state into the running ParaView session. Shared
  /// between the legacy loader (which calls the private helpers
  /// directly) and the new-format loader in SaveLoadStateReaction.
  /// If @a pipeline is non-null, camera state is applied on
  /// Pipeline::executionFinished so it lands after any
  /// resetCameraIfFirstSink adjustments.
  /// @a viewIdMap, if non-null, is populated with the legacy
  /// view-id → restored-proxy mapping so the caller can bind sinks
  /// back to their saved views.
  static void restoreViewsLayoutsAndPalette(
    const QJsonObject& state, Pipeline* pipeline,
    QMap<int, vtkSMViewProxy*>* viewIdMap = nullptr);

private:
  struct LoadContext
  {
    Pipeline* pipeline = nullptr;
    vtkSMViewProxy* view = nullptr;
    QDir stateDir;
    h5::H5ReadWrite* h5 = nullptr;       // non-null for .tvh5 loads
    QString activeDataSourceId;          // /data/tomography source (tvh5)
    QStringList skippedOperators;
    QStringList skippedModules;
    /// Legacy view id → restored view proxy, built after
    /// pqApplicationCore::loadState() recreates the views.
    QMap<int, vtkSMViewProxy*> viewIdMap;
  };

  /// Build a SourceNode from a top-level DataSource JSON entry. For .tvsm
  /// this resolves reader.fileNames relative to stateDir; for .tvh5 it
  /// reads voxel data from the HDF5 group keyed by the DataSource id.
  static SourceNode* buildSource(const QJsonObject& dsJson,
                                 LoadContext& ctx);

  /// Walk the pipeline rooted at @a ds (a DataSource JSON object) and
  /// attach operators/modules to @a upstream's primary output port.
  ///
  /// The ``ds`` object may be:
  ///   - a top-level DataSource (has a "reader" section), or
  ///   - a child DataSource embedded inside an operator's "dataSources"
  ///     array (no reader; inherits data from the producing transform).
  static void walkDataSource(Node* upstream, const QJsonObject& ds,
                             LoadContext& ctx);

  /// Create a TransformNode for the given operator JSON entry. Returns
  /// null and appends to ctx.skippedOperators for unknown types.
  static TransformNode* buildOperator(const QJsonObject& opJson,
                                      LoadContext& ctx);

  /// Create and configure a LegacyModuleSink for the given module JSON
  /// entry. Returns null and appends to ctx.skippedModules for unknown
  /// types. The sink is added to ``pipeline`` and linked to @a sourcePort
  /// via a SinkGroupNode (one SinkGroupNode per upstream port).
  static LegacyModuleSink* buildModule(const QJsonObject& moduleJson,
                                       OutputPort* sourcePort,
                                       SinkGroupNode* group,
                                       LoadContext& ctx);

  /// Apply DataSource-level metadata (spacing, origin, units, colormap
  /// points...) to the VolumeData on @a node's primary output port.
  /// No-op if the port has no data yet.
  static void applyDataSourceMetadata(Node* node,
                                      const QJsonObject& dsJson);

  /// Pick the active view for sinks to render into. Used as a fallback
  /// when a module's viewId isn't present in ctx.viewIdMap.
  static vtkSMViewProxy* resolveView(LoadContext& ctx);

  /// Recreate ParaView views + layout from @a state ("views", "layouts")
  /// via pqApplicationCore::loadState(), preserving legacy global IDs,
  /// and populate ctx.viewIdMap from the ProxyLocator so later
  /// buildModule() calls can look up the right view per module.
  /// Returns false if no views[] is present or XML construction/parse
  /// fails — caller should fall back to the app's current active view.
  static bool restoreViewsAndLayouts(const QJsonObject& state,
                                      LoadContext& ctx);

  /// Apply a legacy top-level paletteColor (array of 3 doubles) to the
  /// ColorPalette settings proxy's BackgroundColor. Views that set
  /// UseColorPaletteForBackground=1 will then pick up this color.
  static void applyPaletteColor(const QJsonArray& color);

  /// Apply a legacy view JSON (camera, axes visibility, background,
  /// interaction/orthographic modes, ...) to @a view. Multi-view state
  /// is not restored — the caller selects a single representative
  /// view (the one marked "active", or the first, falling back to the
  /// application's current active view). When @a cameraToo is false
  /// the camera position / focal point / viewUp / angles / parallel
  /// scale are skipped — used when state needs to land before any
  /// sink's first consume has had a chance to auto-reset the camera.
  static void applyViewState(vtkSMViewProxy* view,
                             const QJsonObject& viewJson,
                             bool cameraToo = true);

  /// Schedule applyViewState() to fire once on Pipeline::executionFinished
  /// so the restored camera lands after LegacyModuleSink::resetCameraIfFirstSink
  /// runs on the first consume. Single-view path.
  static void scheduleViewStateApply(const QJsonObject& state,
                                     vtkSMViewProxy* view,
                                     Pipeline* pipeline);

  /// Multi-view variant: schedule applyViewState() for every view in
  /// state["views"] that resolves through ctx.viewIdMap. Falls back to
  /// scheduleViewStateApply() (single-view, active view) when the id
  /// map is empty.
  static void scheduleViewStatesApply(const QJsonObject& state,
                                      LoadContext& ctx);
};

} // namespace pipeline
} // namespace tomviz

#endif
