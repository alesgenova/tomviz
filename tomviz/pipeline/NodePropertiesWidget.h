/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineNodePropertiesWidget_h
#define tomvizPipelineNodePropertiesWidget_h

#include "EditNodeWidget.h"
#include "ParameterInterfaceBuilder.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>

namespace tomviz {
namespace pipeline {

/// A JSON-driven parameter editing widget built by ParameterInterfaceBuilder.
/// Does not own any buttons — the wrapper (NodePropertiesPanel or
/// NodeEditDialog) provides Apply/OK/Cancel.
class NodePropertiesWidget
  : public EditNodeWidget
{
  Q_OBJECT

public:
  NodePropertiesWidget(const QString& jsonDescription,
                       const QMap<QString, QVariant>& currentValues,
                       const QList<PortScalars>& portScalars = {},
                       QWidget* parent = nullptr);

  QMap<QString, QVariant> values() const;

  void applyChangesToOperator() override;

signals:
  /// Emitted by applyChangesToOperator(), carrying the current values.
  /// The node connects to this signal to update its parameters.
  void applyRequested(const QMap<QString, QVariant>& values);

private:
  QWidget* m_innerWidget = nullptr;
};

} // namespace pipeline
} // namespace tomviz

#endif
