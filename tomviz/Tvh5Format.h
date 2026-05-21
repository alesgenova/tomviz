/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizTvh5Format_h
#define tomvizTvh5Format_h

#include <QJsonObject>

#include <string>

namespace tomviz {
namespace pipeline {
class Pipeline;
}

/// Saves the new-format (schemaVersion >= 2) Tomviz state file into an
/// HDF5 container. The pipeline graph JSON lives at `/tomviz_state`;
/// raw payload bytes for each source-node output port live at
/// `/data/<nodeId>/<portName>`. Port entries in the JSON gain a
/// `dataRef: { container: "h5", path: ... }` field pointing at those
/// groups so a future loader can resolve them.
///
/// @a extraState is merged into the final JSON before writing — callers
/// that want views/layouts/paletteColor in the file must assemble them
/// via ViewsLayoutsSerializer (the running session requires
/// pqApplicationCore, which unit tests don't initialize).
///
/// Load is not yet implemented — old `.tvh5` files still go through
/// `LegacyStateLoader::loadFromH5`.
class Tvh5Format
{
public:
  static bool write(const std::string& fileName,
                    pipeline::Pipeline* pipeline,
                    const QJsonObject& extraState = QJsonObject());

  /// Read the `/tomviz_state` JSON blob from a .tvh5 file. Returns an
  /// empty object on failure or if the dataset is missing.
  static QJsonObject readState(const std::string& fileName);

  /// Walk @a pipelineJson.nodes, and for every output port whose
  /// `dataRef` points at an HDF5 group inside @a fileName, read the
  /// payload from that group and attach it to the matching port on
  /// @a pipeline.  Volume-typed ports are restored as VolumeData (via
  /// EmdFormat::readNode); Table-typed ports as vtkTable;
  /// Molecule-typed ports as vtkMolecule.  Covers source nodes (their
  /// saved input), transforms whose outputs the user persisted (so
  /// downstream doesn't need to re-run), and any other node with a
  /// `persistent` port.
  /// Suitable as a PipelineStateIO::PreExecuteHook closure.
  static void populatePayloadData(pipeline::Pipeline* pipeline,
                                  const QJsonObject& pipelineJson,
                                  const std::string& fileName);
};

} // namespace tomviz

#endif // tomvizTvh5Format_h
