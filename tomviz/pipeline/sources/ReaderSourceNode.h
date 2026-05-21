/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineReaderSourceNode_h
#define tomvizPipelineReaderSourceNode_h

#include "SourceNode.h"

#include <QJsonObject>
#include <QStringList>

namespace tomviz {
namespace pipeline {

/// A source node that reads file(s) via tomviz::readImageData() and
/// exposes the resulting VolumeData on its "volume" output port.
class ReaderSourceNode : public SourceNode
{
  Q_OBJECT

public:
  ReaderSourceNode(QObject* parent = nullptr);
  ~ReaderSourceNode() override = default;

  /// Set the file path(s) to read. Does not itself touch disk — the
  /// actual read happens in execute(), via tomviz::readImageData().
  void setFileNames(const QStringList& fileNames);
  QStringList fileNames() const;

  /// Extra options forwarded to readImageData — carries the ParaView
  /// reader descriptor (for round-tripping), subsampleSettings, and
  /// a tvh5NodePath when reading a .tvh5 group directly. Optional.
  void setReaderOptions(const QJsonObject& options);
  QJsonObject readerOptions() const;

  /// Read the file(s) and set the result on the output port.
  bool execute() override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

private:
  QStringList m_fileNames;
  QJsonObject m_readerOptions;
};

} // namespace pipeline
} // namespace tomviz

#endif
