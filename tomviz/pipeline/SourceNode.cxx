/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SourceNode.h"

#include "OutputPort.h"

#include <QJsonObject>

namespace tomviz {
namespace pipeline {

SourceNode::SourceNode(QObject* parent) : Node(parent) {}

QIcon SourceNode::icon() const
{
  return QIcon(QStringLiteral(":/pqWidgets/Icons/pqHome.svg"));
}

OutputPort* SourceNode::addOutput(const QString& name, PortType type)
{
  // Source outputs default to persistent: the root payload is typically
  // expensive to recreate (file read, network fetch, simulator run), so
  // we pin it for the lifetime of the node rather than re-executing on
  // every downstream re-attach.
  auto* port = addOutputPort(name, type);
  port->setPersistent(true);
  return port;
}

void SourceNode::setOutputData(const QString& portName, const PortData& data)
{
  auto* port = outputPort(portName);
  if (port) {
    port->setData(data);
    markCurrent();
    // Mark all downstream nodes as stale
    for (auto* downstream : downstreamNodes()) {
      downstream->markStale();
    }
  }
}

bool SourceNode::execute()
{
  // A source node is current if any output has data
  for (auto* port : outputPorts()) {
    if (port->hasData()) {
      markCurrent();
      return true;
    }
  }
  return true;
}

QJsonObject SourceNode::serialize() const
{
  return Node::serialize();
}

bool SourceNode::deserialize(const QJsonObject& json)
{
  // For the base SourceNode (instances created by LoadDataReaction,
  // MergeImagesReaction, etc.), output ports aren't declared in a
  // constructor — recreate them from the file's outputPorts map so the
  // link-resolution pass can find them. Subclasses that pre-declare
  // their ports in their constructor find matching entries and this
  // becomes a no-op.
  if (json.contains(QStringLiteral("outputPorts"))) {
    auto outputs = json.value(QStringLiteral("outputPorts")).toObject();
    for (auto it = outputs.constBegin(); it != outputs.constEnd(); ++it) {
      if (outputPort(it.key())) {
        continue;
      }
      auto entry = it.value().toObject();
      PortType type =
        portTypeFromString(entry.value(QStringLiteral("type")).toString());
      addOutput(it.key(), type);
    }
  }
  return Node::deserialize(json);
}

} // namespace pipeline
} // namespace tomviz
