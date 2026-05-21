/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineNodePropertiesPanel_h
#define tomvizPipelineNodePropertiesPanel_h

#include <QWidget>

class QPushButton;

namespace tomviz {
namespace pipeline {

class EditNodeWidget;
class Node;
class Pipeline;

/// Panel wrapper for node properties shown in the properties dock.
/// Creates the node's EditNodeWidget, adds an Apply button, and handles
/// applying parameters + pipeline re-execution. Gating of the editor
/// (e.g. waiting for input data) is handled inside the EditNodeWidget
/// itself; this wrapper just toggles the Apply button based on
/// canApply() and the pipeline's executing state.
///
/// Suppresses the parametersApplied → execute() auto-wiring while alive
/// to avoid double execution / deadlock with ThreadedExecutor.
class NodePropertiesPanel : public QWidget
{
  Q_OBJECT

public:
  NodePropertiesPanel(Node* node, Pipeline* pipeline,
                      QWidget* parent = nullptr);
  ~NodePropertiesPanel() override;

private slots:
  void apply();

private:
  void refreshApplyEnablement();

  Node* m_node;
  Pipeline* m_pipeline;
  EditNodeWidget* m_editWidget = nullptr;
  QPushButton* m_applyButton = nullptr;
};

} // namespace pipeline
} // namespace tomviz

#endif
