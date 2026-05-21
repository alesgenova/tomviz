/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SinkNode.h"

#include "InputPort.h"
#include "Link.h"
#include "OutputPort.h"

namespace tomviz {
namespace pipeline {

SinkNode::SinkNode(QObject* parent) : Node(parent) {}

InputPort* SinkNode::addInput(const QString& name, PortTypes acceptedTypes)
{
  auto* port = addInputPort(name, acceptedTypes);
  connectUpstreamIntermediate(port);
  // Re-connect when the link changes (new connection / disconnection).
  connect(port, &InputPort::connectionChanged, this, [this, port]() {
    if (port->link()) {
      connectUpstreamIntermediate(port);
    } else {
      // Drop the retained input for the disconnected port so its
      // shared_ptr reference to the upstream payload releases.
      m_retainedInputs.remove(port->name());
      onInputDisconnected(port);
    }
  });
  return port;
}

void SinkNode::connectUpstreamIntermediate(InputPort* port)
{
  if (port->link() && port->link()->from()) {
    connect(port->link()->from(), &OutputPort::intermediateDataApplied,
            this, &SinkNode::onIntermediateData,
            Qt::UniqueConnection);
  }
}

void SinkNode::onIntermediateData()
{
  QMap<QString, PortData> inputs;
  for (auto* port : inputPorts()) {
    if (port->hasData()) {
      inputs[port->name()] = port->data();
    }
  }
  if (!inputs.isEmpty()) {
    prepareConsume(inputs);
    bool success = consume(inputs);
    postConsume(success);
  }
}

bool SinkNode::execute()
{
  setExecState(NodeExecState::Running);

  // The executor has set each input port's handle to a strong ref to
  // the upstream payload. Stash those handles (sinks need their input
  // alive after the executor's in-flight refs go out of scope), and
  // build the value-copy map that consume() expects.
  QMap<QString, std::shared_ptr<PortData>> handles;
  QMap<QString, PortData> inputs;
  for (auto* port : inputPorts()) {
    const auto& h = port->handle();
    if (h) {
      handles[port->name()] = h;
      inputs[port->name()] = *h;
    } else {
      inputs[port->name()] = port->data();
    }
  }

  prepareConsume(inputs);
  bool success = consume(inputs);
  postConsume(success);

  if (success) {
    m_retainedInputs = handles;
    markCurrent();
    setExecState(NodeExecState::Idle);
  } else {
    setExecState(NodeExecState::Failed);
  }

  return success;
}

} // namespace pipeline
} // namespace tomviz
