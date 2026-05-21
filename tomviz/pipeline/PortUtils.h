/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePortUtils_h
#define tomvizPipelinePortUtils_h

#include "Node.h"
#include "OutputPort.h"
#include "PortData.h"

#include <any>

namespace tomviz {
namespace pipeline {

/// Get typed data from a node's output port.
/// If portName is empty, uses the first output port.
/// Returns default-constructed T (e.g., nullptr for shared_ptr) on failure.
template <typename T>
T getOutputData(const Node* node, const QString& portName = {})
{
  if (!node) {
    return T{};
  }
  const auto& ports = node->outputPorts();
  OutputPort* port = portName.isEmpty()
    ? (ports.isEmpty() ? nullptr : ports.first())
    : node->outputPort(portName);
  if (!port || !port->hasData()) {
    return T{};
  }
  try {
    return port->data().value<T>();
  } catch (const std::bad_any_cast&) {
    return T{};
  }
}

} // namespace pipeline
} // namespace tomviz

#endif
