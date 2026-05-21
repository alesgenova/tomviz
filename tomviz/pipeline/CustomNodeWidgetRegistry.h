/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineCustomNodeWidgetRegistry_h
#define tomvizPipelineCustomNodeWidgetRegistry_h

#include "PortData.h"

#include <QMap>
#include <QString>

#include <functional>

class QWidget;

namespace tomviz {
namespace pipeline {

class CustomPythonNodeWidget;

/// Registration info for a custom widget that replaces the
/// auto-generated parameter UI on a Python node (source or transform).
/// Schema-v1 (LegacyPythonTransform) and schema-v2 (PythonTransform /
/// PythonSource) nodes share this registry and look up widgets by the
/// JSON description's ``widget`` field.
struct CustomWidgetInfo
{
  /// True if widget creation should wait until the host node's input
  /// ports carry data (typical for transform widgets that render a
  /// preview over an input volume). Always set false for source-shape
  /// widgets — there are no inputs to wait for. Independent of whether
  /// the widget actually consumes the inputs at construction time: a
  /// transform widget that ignores ``inputs`` registers with
  /// needsData=false.
  bool needsData = false;

  /// Factory: builds the widget from the host node's current input
  /// data. Widgets that don't consume inputs ignore the @a inputs
  /// parameter; sources always receive an empty map.
  std::function<CustomPythonNodeWidget*(
    const QMap<QString, PortData>& inputs, QWidget* parent)>
    create;
};

/// Register a custom widget factory under @a id. Calling again with
/// the same id overwrites the previous registration.
void registerCustomNodeWidget(const QString& id,
                              const CustomWidgetInfo& info);

/// Convenience overload: register a widget whose constructor matches
/// the standard ``T(const QMap<QString, PortData>& inputs, QWidget*
/// parent)`` signature. Most existing widgets use this — the explicit
/// CustomWidgetInfo overload is the escape hatch for unusual cases.
template <typename T>
void registerCustomNodeWidget(const QString& id, bool needsData = false)
{
  registerCustomNodeWidget(
    id, { needsData,
          [](const QMap<QString, PortData>& inputs, QWidget* parent)
            -> CustomPythonNodeWidget* {
            return new T(inputs, parent);
          } });
}

/// Look up a previously-registered widget. Returns nullptr if no
/// widget is registered under @a id. The returned pointer is owned by
/// the registry and remains valid for the program's lifetime.
const CustomWidgetInfo* findCustomNodeWidget(const QString& id);

} // namespace pipeline
} // namespace tomviz

#endif
