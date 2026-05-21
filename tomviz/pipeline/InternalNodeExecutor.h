/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineInternalNodeExecutor_h
#define tomvizPipelineInternalNodeExecutor_h

#include "NodeExecutor.h"

namespace tomviz {
namespace pipeline {

/// Default NodeExecutor: runs the node in-process by calling
/// node->execute() directly. Stateless — exposed as a process-wide
/// singleton via instance(), which the pipeline executors use as the
/// fallback when a node has no per-node executor set.
class InternalNodeExecutor : public NodeExecutor
{
  Q_OBJECT

public:
  static InternalNodeExecutor& instance();

  bool execute(Node* node) override;

  /// Returns an empty string — the internal executor is the implicit
  /// default and is never written into the node's serialized form.
  QString type() const override;

private:
  InternalNodeExecutor();
  ~InternalNodeExecutor() override = default;
};

} // namespace pipeline
} // namespace tomviz

#endif
