/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineDeferredLinkInfo_h
#define tomvizPipelineDeferredLinkInfo_h

#include <QList>

namespace tomviz {
namespace pipeline {

class InputPort;
class OutputPort;

/// Captures deferred link operations for a pending transform insertion.
/// When the transform is accepted, old links are removed and new links
/// are created to complete the insertion.
struct DeferredLinkInfo
{
  struct LinkEndpoints
  {
    OutputPort* from = nullptr;
    InputPort* to = nullptr;
  };

  /// Old links to remove on commit (looked up by matching from/to ports).
  QList<LinkEndpoints> linksToBreak;

  /// New output links to create from the new transform's output ports.
  QList<LinkEndpoints> linksToCreate;

  bool isEmpty() const
  {
    return linksToBreak.isEmpty() && linksToCreate.isEmpty();
  }
};

} // namespace pipeline
} // namespace tomviz

#endif
