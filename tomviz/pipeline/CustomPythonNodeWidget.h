/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineCustomPythonNodeWidget_h
#define tomvizPipelineCustomPythonNodeWidget_h

#include <QMap>
#include <QString>
#include <QVariant>
#include <QWidget>

namespace tomviz {
namespace pipeline {

/// Base class for custom parameter widgets that replace the
/// auto-generated parameter UI for specific Python nodes (sources or
/// transforms). Concrete subclasses are registered with
/// :func:`registerCustomNodeWidget` keyed on the JSON description's
/// ``widget`` field.
class CustomPythonNodeWidget : public QWidget
{
  Q_OBJECT

public:
  CustomPythonNodeWidget(QWidget* parent = nullptr);
  ~CustomPythonNodeWidget() override;

  virtual void getValues(QMap<QString, QVariant>& map) = 0;
  virtual void setValues(const QMap<QString, QVariant>& map) = 0;

  /// Keep a copy of the current script (including edits) in case the
  /// custom widget needs to use it (e.g. for running test Python code).
  virtual void setScript(const QString& script) { m_script = script; }

  /// Called when the operator is applied (e.g. to persist settings).
  virtual void writeSettings() {}

protected:
  QString m_script;
};

} // namespace pipeline
} // namespace tomviz

#endif
