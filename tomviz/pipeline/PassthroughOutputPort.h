/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePassthroughOutputPort_h
#define tomvizPipelinePassthroughOutputPort_h

#include "OutputPort.h"

namespace tomviz {
namespace pipeline {

class SinkNode;

/// An OutputPort proxy that delegates data access and signal forwarding
/// to a source OutputPort.  Used by SinkGroupNode so that grouped sinks
/// see the upstream data without any copying.
class PassthroughOutputPort : public OutputPort
{
  Q_OBJECT

public:
  PassthroughOutputPort(const QString& name, PortType type,
                        QObject* parent = nullptr);

  /// Set (or change) the upstream port this proxy delegates to.
  /// Disconnects signals from the previous source and connects the new one.
  /// Pass nullptr to disconnect.
  void setSource(OutputPort* source);
  OutputPort* source() const;

  PortData data() const override;
  std::shared_ptr<PortData> materialize() override;
  bool hasData() const override;
  bool isStale() const override;
  DataLocation dataLocation() const override;

  /// A passthrough owns no data of its own — it forwards from m_source.
  /// Always report non-persistent so state-file writers skip it (no
  /// duplicated voxel embeds) and stay immune to old state files whose
  /// persistent flag would otherwise flip the base m_persistent.
  bool isPersistent() const override { return false; }

  /// Only SinkNodes may connect to a passthrough port.
  bool canAcceptLink(InputPort* to) const override;

private:
  OutputPort* m_source = nullptr;
};

} // namespace pipeline
} // namespace tomviz

#endif
