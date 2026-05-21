/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Link.h"

#include "InputPort.h"
#include "OutputPort.h"
#include "PortType.h"

namespace tomviz {
namespace pipeline {

Link::Link(OutputPort* from, InputPort* to, QObject* parent)
  : QObject(parent), m_from(from), m_to(to)
{
  if (m_from) {
    m_from->addLink(this);
  }
  if (m_to) {
    m_to->setLink(this);
  }
  // Compute initial validity
  m_valid = isConnected() && m_to->canConnectTo(m_from);
}

Link::~Link()
{
  emit aboutToBeRemoved();
  if (m_from) {
    m_from->removeLink(this);
  }
  if (m_to) {
    m_to->setLink(nullptr);
  }
}

OutputPort* Link::from() const
{
  return m_from;
}

InputPort* Link::to() const
{
  return m_to;
}

bool Link::isValid() const
{
  return m_valid;
}

bool Link::isConnected() const
{
  return m_from && m_to;
}

void Link::recheck()
{
  bool valid = isConnected() && m_to->canConnectTo(m_from);
  if (m_valid != valid) {
    m_valid = valid;
    emit validityChanged(valid);
  }
}

} // namespace pipeline
} // namespace tomviz
