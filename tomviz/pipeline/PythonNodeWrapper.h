/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePythonNodeWrapper_h
#define tomvizPipelinePythonNodeWrapper_h

// Qt's `slots` macro conflicts with Python's object.h. Includers must
// already deal with this; we mirror the dance here so they can include
// the header in any order.
#pragma push_macro("slots")
#undef slots
#include <pybind11/pybind11.h>
#pragma pop_macro("slots")

#include <QString>

namespace tomviz {
namespace pipeline {

class Node;

/// Build the Python `_operator_wrapper` instance that exposes the
/// standard progress / cancel / completion / progress-data API for an
/// operator script bound to @a node. Used by Python-bearing node
/// types (LegacyPythonTransform today; future PythonNode) to avoid
/// duplicating the pybind11 wiring.
///
/// @a primaryPortName is stamped onto the wrapper as the
/// `primary_port` attribute. tomviz.operators.Progress reads it to
/// translate a bare `self.progress.data = X` into the multi-port
/// `{port_name: payload}` dict the wrapper expects, so the C++ side
/// only ever has to handle the dict form. Pass an empty string to
/// disable the bare-value route — operators must then always pass a
/// dict for previews to land.
pybind11::object createNodeWrapper(
  Node* node, const QString& primaryPortName = QString());

} // namespace pipeline
} // namespace tomviz

#endif
