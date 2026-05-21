/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineEditNodeWidget_h
#define tomvizPipelineEditNodeWidget_h

#include <QWidget>

namespace tomviz {
namespace pipeline {

/// Base class for custom node editing widgets.
///
/// Subclasses implement applyChangesToOperator() to commit the current UI
/// state to the node's parameters.  The wrapper (NodePropertiesPanel
/// or NodeEditDialog) owns the Apply/OK/Cancel buttons and calls this
/// slot when the user clicks them.
class EditNodeWidget : public QWidget
{
  Q_OBJECT

public:
  EditNodeWidget(QWidget* parent = nullptr);
  ~EditNodeWidget() override;

  /// Whether an Apply (or OK) on this widget would do useful work.
  /// Wrappers gate the Apply/OK button enabled state on this. Default
  /// is true; subclasses that render in a degraded state (e.g. a
  /// GatedEditorWidget showing the not-ready warning) override to
  /// return false until they have a real editor to commit.
  virtual bool canApply() const { return true; }

signals:
  /// Emitted when the return value of canApply() may have changed.
  /// Wrappers listen to this to refresh their Apply/OK enablement.
  void canApplyChanged();

public slots:
  /// Apply the current widget state to the node's parameters.
  virtual void applyChangesToOperator() = 0;
};

} // namespace pipeline
} // namespace tomviz

#endif
