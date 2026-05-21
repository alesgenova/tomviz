/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Port.h"

#include "Node.h"

namespace tomviz {
namespace pipeline {

Port::Port(const QString& name, QObject* parent)
  : QObject(parent), m_name(name)
{}

QString Port::name() const
{
  return m_name;
}

void Port::setName(const QString& name)
{
  m_name = name;
}

Node* Port::node() const
{
  return qobject_cast<Node*>(parent());
}

} // namespace pipeline
} // namespace tomviz
