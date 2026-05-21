/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "NodePropertiesPanel.h"

#include "EditNodeWidget.h"
#include "Node.h"
#include "Pipeline.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace tomviz {
namespace pipeline {

NodePropertiesPanel::NodePropertiesPanel(Node* node, Pipeline* pipeline,
                                         QWidget* parent)
  : QWidget(parent), m_node(node), m_pipeline(pipeline)
{
  // Suppress the auto-execute wiring so this panel controls execution.
  QObject::disconnect(m_node, &Node::parametersApplied, m_pipeline, nullptr);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  m_editWidget = node->createPropertiesWidget(pipeline, this);
  if (!m_editWidget) {
    return;
  }

  auto* scrollArea = new QScrollArea(this);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setWidgetResizable(true);
  scrollArea->setWidget(m_editWidget);
  layout->addWidget(scrollArea, 1);

  auto* buttonBox = new QDialogButtonBox(
    QDialogButtonBox::Apply, Qt::Horizontal, this);
  m_applyButton = buttonBox->button(QDialogButtonBox::Apply);
  connect(m_applyButton, &QPushButton::clicked,
          this, &NodePropertiesPanel::apply);
  layout->addWidget(buttonBox);

  connect(m_editWidget, &EditNodeWidget::canApplyChanged,
          this, &NodePropertiesPanel::refreshApplyEnablement);
  connect(m_pipeline, &Pipeline::executionStarted,
          this, &NodePropertiesPanel::refreshApplyEnablement);
  connect(m_pipeline, &Pipeline::executionFinished,
          this, &NodePropertiesPanel::refreshApplyEnablement);
  refreshApplyEnablement();
}

NodePropertiesPanel::~NodePropertiesPanel()
{
  // Restore the auto-wiring if the node is still in the pipeline.
  if (m_node && m_pipeline && m_pipeline->nodes().contains(m_node)) {
    connect(m_node, &Node::parametersApplied, m_pipeline,
            [pip = m_pipeline]() { pip->execute(); });
  }
}

void NodePropertiesPanel::refreshApplyEnablement()
{
  if (!m_applyButton || !m_editWidget) {
    return;
  }
  m_applyButton->setEnabled(m_editWidget->canApply() &&
                            !m_pipeline->isExecuting());
}

void NodePropertiesPanel::apply()
{
  if (m_editWidget) {
    m_editWidget->applyChangesToOperator();
  }
  if (m_node) {
    m_node->markStale();
  }
  if (m_pipeline) {
    m_pipeline->execute();
  }
}

} // namespace pipeline
} // namespace tomviz
