/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PassthroughOutputPort.h"

#include "InputPort.h"
#include "SinkNode.h"

namespace tomviz {
namespace pipeline {

PassthroughOutputPort::PassthroughOutputPort(const QString& name,
                                             PortType type,
                                             QObject* parent)
  : OutputPort(name, type, parent)
{}

void PassthroughOutputPort::setSource(OutputPort* source)
{
  if (m_source == source) {
    return;
  }

  // Disconnect old source signals.
  if (m_source) {
    disconnect(m_source, nullptr, this, nullptr);
  }

  m_source = source;

  // Forward all relevant signals from the new source.
  if (m_source) {
    connect(m_source, &OutputPort::dataChanged,
            this, &OutputPort::dataChanged);
    connect(m_source, &OutputPort::intermediateDataApplied,
            this, &OutputPort::intermediateDataApplied);
    connect(m_source, &OutputPort::metadataChanged,
            this, &OutputPort::metadataChanged);
    connect(m_source, &OutputPort::staleChanged,
            this, &OutputPort::staleChanged);
    connect(m_source, &OutputPort::effectiveTypeChanged,
            this, &OutputPort::effectiveTypeChanged);
    connect(m_source, &OutputPort::dataLocationChanged,
            this, &OutputPort::dataLocationChanged);
  }
}

OutputPort* PassthroughOutputPort::source() const
{
  return m_source;
}

PortData PassthroughOutputPort::data() const
{
  if (m_source) {
    return m_source->data();
  }
  return PortData();
}

std::shared_ptr<PortData> PassthroughOutputPort::materialize()
{
  if (m_source) {
    return m_source->materialize();
  }
  return nullptr;
}

bool PassthroughOutputPort::hasData() const
{
  if (m_source) {
    return m_source->hasData();
  }
  return false;
}

bool PassthroughOutputPort::isStale() const
{
  if (m_source) {
    return m_source->isStale();
  }
  return true;
}

DataLocation PassthroughOutputPort::dataLocation() const
{
  if (m_source) {
    return m_source->dataLocation();
  }
  return DataLocation::None;
}

bool PassthroughOutputPort::canAcceptLink(InputPort* to) const
{
  if (!to || !to->node()) {
    return false;
  }
  return qobject_cast<SinkNode*>(to->node()) != nullptr;
}

} // namespace pipeline
} // namespace tomviz
