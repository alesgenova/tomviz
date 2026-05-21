/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePythonTransform_h
#define tomvizPipelinePythonTransform_h

#include "PythonNodeBackend.h"
#include "TransformNode.h"

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVariant>

namespace tomviz {
namespace pipeline {

/// A schema-v2 Python transform node. Owns a PythonNodeBackend that
/// handles JSON parsing, parameter management, and execution; this
/// shell only wires the backend into the TransformNode lifecycle.
///
/// The user-facing Python class inherits from
/// tomviz.nodes.TransformNode and implements
/// ``transform(self, inputs, **params) -> dict``.
class PythonTransform : public TransformNode
{
  Q_OBJECT

public:
  PythonTransform(QObject* parent = nullptr);
  ~PythonTransform() override = default;

  void setJSONDescription(const QString& json);
  QString jsonDescription() const;

  void setScript(const QString& script);
  QString scriptSource() const;

  void setParameter(const QString& name, const QVariant& value);
  QVariant parameter(const QString& name) const;
  QMap<QString, QVariant> parameters() const;

  QString operatorName() const;

  /// The custom widget id from the JSON description's optional
  /// ``widget`` field (empty when none).
  QString customWidgetID() const;

  bool hasPropertiesWidget() const override;
  EditNodeWidget* createPropertiesWidget(Pipeline* pipeline,
                                         QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override;

private:
  PythonNodeBackend m_backend;
};

} // namespace pipeline
} // namespace tomviz

#endif
