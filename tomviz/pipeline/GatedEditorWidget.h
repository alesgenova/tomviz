/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineGatedEditorWidget_h
#define tomvizPipelineGatedEditorWidget_h

#include "EditNodeWidget.h"
#include "PortData.h"

#include <QList>

#include <functional>
#include <memory>

class QVBoxLayout;

namespace tomviz {
namespace pipeline {

class InputsNotReadyWidget;
class Node;
class Pipeline;

/// Reusable wrapper for node editors that need all input data to be
/// linked, current, and in memory before they can render. Shows an
/// InputsNotReadyWidget while inputs are missing; on the user's "Run
/// Pipeline" click runs executeUpstreamOf(target) and, on completion,
/// materializes the input ports and builds the inner editor.
///
/// Nodes wrap their existing editor construction in a small factory
/// closure that's only invoked once readiness is satisfied — so the
/// closure can assume the upstream data is materialized.
class GatedEditorWidget : public EditNodeWidget
{
  Q_OBJECT

public:
  using EditorFactory = std::function<EditNodeWidget*(QWidget* parent)>;

  GatedEditorWidget(Node* node, Pipeline* pipeline,
                    EditorFactory factory, QWidget* parent = nullptr);
  ~GatedEditorWidget() override = default;

  bool canApply() const override;
  void applyChangesToOperator() override;

private:
  void onRunRequested();
  void onExecutionFinished();
  bool inputsReady() const;
  void buildInner();

  Node* m_node;
  Pipeline* m_pipeline;
  EditorFactory m_factory;
  QVBoxLayout* m_layout = nullptr;
  InputsNotReadyWidget* m_notReadyWidget = nullptr;
  EditNodeWidget* m_inner = nullptr;
  QList<std::shared_ptr<PortData>> m_inputPins;
};

} // namespace pipeline
} // namespace tomviz

#endif
