/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineSinkGroupNode_h
#define tomvizPipelineSinkGroupNode_h

#include "Node.h"

namespace tomviz {
namespace pipeline {

class PassthroughOutputPort;
class SinkNode;

/// A pipeline node that acts as a transparent grouping container for sinks.
///
/// Inherits directly from Node (not TransformNode or SinkNode) because it
/// has output ports (unlike SinkNode) but performs no data transformation
/// (unlike TransformNode).  Its PassthroughOutputPort delegates data access
/// and signal forwarding to the upstream port with zero copies.
///
/// Moving the group to a different upstream port is atomic — only one
/// input link needs to change regardless of how many sinks are in the group.
class SinkGroupNode : public Node
{
  Q_OBJECT

public:
  SinkGroupNode(QObject* parent = nullptr);
  ~SinkGroupNode() override = default;

  /// Add a passthrough port pair.  Creates a matching input port and
  /// PassthroughOutputPort.  Type inference is configured automatically
  /// so the output effective type follows the input.
  void addPassthrough(const QString& name, PortType type);

  bool execute() override;

  /// All sink nodes currently connected to this group's output ports.
  QList<SinkNode*> sinks() const;

  /// Recreates passthrough port pairs from the serialized outputPorts
  /// map before forwarding to Node::deserialize for label / properties.
  bool deserialize(const QJsonObject& json) override;
};

} // namespace pipeline
} // namespace tomviz

#endif
