/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizTransformUtils_h
#define tomvizTransformUtils_h

namespace tomviz {
namespace pipeline {
class TransformNode;
}

/// Insert a TransformNode into the active pipeline at the current tip port.
///
/// Handles:
///  - Ctrl held → add node unconnected
///  - Active link selected → insert between nodes
///  - Otherwise → append at tip output port
///  - If hasPropertiesWidget() → show NodeEditDialog with deferred links
///  - Otherwise → complete insertion and execute
///
/// Returns true on success.  On failure the transform is deleted.
bool insertTransformIntoPipeline(pipeline::TransformNode* transform);

} // namespace tomviz

#endif
