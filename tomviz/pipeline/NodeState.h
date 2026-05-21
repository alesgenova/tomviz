/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineNodeState_h
#define tomvizPipelineNodeState_h

namespace tomviz {
namespace pipeline {

enum class NodeState
{
  New,
  Stale,
  Current
};

} // namespace pipeline
} // namespace tomviz

#endif
