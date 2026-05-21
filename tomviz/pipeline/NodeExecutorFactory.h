/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineNodeExecutorFactory_h
#define tomvizPipelineNodeExecutorFactory_h

#include <QHash>
#include <QString>

#include <functional>

namespace tomviz {
namespace pipeline {

class NodeExecutor;

/// Maps the `type` field stored in a node's serialized "executor" block
/// back to a NodeExecutor instance. Mirrors the role of NodeFactory for
/// nodes themselves.
class NodeExecutorFactory
{
public:
  using Creator = std::function<NodeExecutor*()>;

  static NodeExecutorFactory& instance();

  void registerType(const QString& type, Creator creator);
  NodeExecutor* create(const QString& type) const;

  /// Register built-in NodeExecutor implementations (currently the
  /// ExternalNodeExecutor). Idempotent — safe to call multiple times.
  static void registerBuiltins();

private:
  NodeExecutorFactory() = default;

  QHash<QString, Creator> m_creators;
};

} // namespace pipeline
} // namespace tomviz

#endif
