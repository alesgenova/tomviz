/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineTransformNode_h
#define tomvizPipelineTransformNode_h

#include "Node.h"
#include "PortData.h"

#include <QJsonObject>
#include <QMap>
#include <QString>

class QWidget;

namespace tomviz {
namespace pipeline {

class TransformNode : public Node
{
  Q_OBJECT

public:
  TransformNode(QObject* parent = nullptr);
  ~TransformNode() override = default;

  QIcon icon() const override;

  InputPort* addInput(const QString& name, PortTypes acceptedTypes);
  OutputPort* addOutput(const QString& name, PortType type);

  bool execute() override;

  /// Serialize this transform's persistent state to JSON.
  /// Forwards to Node::serialize(). Subclasses call this base and add
  /// their transform-specific fields.
  QJsonObject serialize() const override;

  /// Apply JSON produced by serialize() (or by a legacy
  /// *Operator::serialize()) to this transform. Returns false on
  /// unrecoverable errors. Forwards to Node::deserialize().
  bool deserialize(const QJsonObject& json) override;

protected:
  virtual QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) = 0;

  /// Apply the application-wide persistence default
  /// (PipelineSettings::transformPersistenceDefault) to @a port.
  /// Subclasses that bypass addOutput() and construct an OutputPort
  /// subclass directly should call this on the port they install so
  /// the global default takes effect uniformly across all transforms.
  void applyDefaultPersistence(OutputPort* port);
};

} // namespace pipeline
} // namespace tomviz

#endif
