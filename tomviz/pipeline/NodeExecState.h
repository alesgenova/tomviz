/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineNodeExecState_h
#define tomvizPipelineNodeExecState_h

namespace tomviz {
namespace pipeline {

enum class NodeExecState
{
  Idle,
  Running,
  Failed,
  Canceled
};

} // namespace pipeline
} // namespace tomviz

#endif
