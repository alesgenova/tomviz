/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineSourceNode_h
#define tomvizPipelineSourceNode_h

#include "Node.h"
#include "PortData.h"

namespace tomviz {
namespace pipeline {

class SourceNode : public Node
{
  Q_OBJECT

public:
  SourceNode(QObject* parent = nullptr);
  ~SourceNode() override = default;

  QIcon icon() const override;

  OutputPort* addOutput(const QString& name, PortType type);
  void setOutputData(const QString& portName, const PortData& data);

  bool execute() override;

  /// Forwards to Node::serialize(). Subclasses call this base and add
  /// their source-specific fields.
  QJsonObject serialize() const override;

  /// Creates output ports declared in the serialized "outputPorts" map
  /// that aren't already present (so the base SourceNode, which adds
  /// no ports in its constructor, can still round-trip through
  /// save/load), then forwards to Node::deserialize() which applies
  /// per-port state including the `metadata` payload.
  bool deserialize(const QJsonObject& json) override;
};

} // namespace pipeline
} // namespace tomviz

#endif
