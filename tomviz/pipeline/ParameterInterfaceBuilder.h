/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineParameterInterfaceBuilder_h
#define tomvizPipelineParameterInterfaceBuilder_h

#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

class QGridLayout;

namespace tomviz {
namespace pipeline {

/// Scalar metadata for one input port — feeds the "select_scalars"
/// parameter widget. The widget picks the matching entry by JSON's
/// "input" field, falling back to the first entry.
struct PortScalars
{
  QString portName;
  QStringList scalarNames;
  QString activeScalar;
};

/// Builds a Qt widget from a JSON parameter description.
/// This is a standalone version of tomviz::InterfaceBuilder that does not
/// depend on DataSource, ActiveObjects, or ModuleManager.
class ParameterInterfaceBuilder : public QObject
{
  Q_OBJECT

public:
  ParameterInterfaceBuilder(QObject* parent = nullptr);

  /// Set the JSON description (operator JSON with "parameters" array).
  void setJSONDescription(const QString& json);
  void setJSONDescription(const QJsonDocument& doc);

  /// Set parameter values to override defaults.
  void setParameterValues(const QMap<QString, QVariant>& values);

  /// Set per-input-port scalar metadata for "select_scalars" parameters.
  /// Each parameter's JSON may include "input": <portName> to pick its
  /// source; if missing or unmatched, the first entry is used. Order
  /// matters: pass the node's input ports in declaration order.
  void setPortScalars(const QList<PortScalars>& ports);

  /// Build the widget tree. Caller owns the returned widget.
  QWidget* buildWidget(QWidget* parent) const;

  /// Extract current parameter values from a widget tree built by this class.
  static QMap<QString, QVariant> parameterValues(const QWidget* widget);

private:
  Q_DISABLE_COPY(ParameterInterfaceBuilder)

  void buildParameterInterface(QGridLayout* layout,
                               QJsonArray& parameters) const;
  void setupEnableAndVisibleStates(const QObject* parent,
                                   QJsonArray& parameters) const;
  void setupEnableStates(const QObject* parent, QJsonArray& parameters,
                         bool visible) const;

  QJsonDocument m_json;
  QMap<QString, QVariant> m_parameterValues;
  QList<PortScalars> m_portScalars;
};

} // namespace pipeline
} // namespace tomviz

#endif
