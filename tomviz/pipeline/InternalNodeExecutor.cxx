/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "InternalNodeExecutor.h"

#include "Node.h"

namespace tomviz {
namespace pipeline {

InternalNodeExecutor::InternalNodeExecutor() : NodeExecutor(nullptr) {}

InternalNodeExecutor& InternalNodeExecutor::instance()
{
  static InternalNodeExecutor s_instance;
  return s_instance;
}

bool InternalNodeExecutor::execute(Node* node)
{
  if (!node) {
    return false;
  }
  return node->execute();
}

QString InternalNodeExecutor::type() const
{
  return QString();
}

} // namespace pipeline
} // namespace tomviz
