/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineNodeFactory_h
#define tomvizPipelineNodeFactory_h

#include <QHash>
#include <QString>

#include <functional>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace tomviz {
namespace pipeline {

class Node;

/// Central registry mapping the state-file "type" discriminator
/// (e.g. "source.reader", "transform.crop", "sink.volume") to Node
/// subclasses. Used by the new-format loader to construct nodes from
/// JSON and by the saver to stamp each node with its type string.
class NodeFactory
{
public:
  /// Register a Node subclass under the given state-file type string.
  /// Safe to call at any time before load/save; later registrations
  /// under the same name overwrite earlier ones.
  template <typename T>
  static void registerType(const QString& typeName)
  {
    auto& inst = instance();
    inst.m_creators[typeName] = []() -> Node* { return new T(); };
    inst.m_typeNames[std::type_index(typeid(T))] = typeName;
  }

  /// Construct a new Node of the registered type, or nullptr if the
  /// type is unknown. Caller takes ownership.
  static Node* create(const QString& typeName);

  /// Reverse lookup: the type string registered for @a node's concrete
  /// class, or an empty string if none has been registered.
  static QString typeName(const Node* node);

  /// Populate the factory with all core Tomviz node types. Idempotent.
  static void registerBuiltins();

private:
  NodeFactory() = default;
  static NodeFactory& instance();

  QHash<QString, std::function<Node*()>> m_creators;
  std::unordered_map<std::type_index, QString> m_typeNames;
};

} // namespace pipeline
} // namespace tomviz

#endif
