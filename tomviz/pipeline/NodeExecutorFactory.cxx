/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "NodeExecutorFactory.h"

#include "ExternalNodeExecutor.h"
#include "NodeExecutor.h"

namespace tomviz {
namespace pipeline {

NodeExecutorFactory& NodeExecutorFactory::instance()
{
  static NodeExecutorFactory s_instance;
  return s_instance;
}

void NodeExecutorFactory::registerType(const QString& type, Creator creator)
{
  m_creators.insert(type, std::move(creator));
}

NodeExecutor* NodeExecutorFactory::create(const QString& type) const
{
  auto it = m_creators.constFind(type);
  if (it == m_creators.constEnd()) {
    return nullptr;
  }
  return (it.value())();
}

void NodeExecutorFactory::registerBuiltins()
{
  static bool done = false;
  if (done) {
    return;
  }
  done = true;

  instance().registerType(
    ExternalNodeExecutor::typeString(),
    []() -> NodeExecutor* { return new ExternalNodeExecutor(); });
}

} // namespace pipeline
} // namespace tomviz
