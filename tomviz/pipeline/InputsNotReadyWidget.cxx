/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "InputsNotReadyWidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace tomviz {
namespace pipeline {

InputsNotReadyWidget::InputsNotReadyWidget(QWidget* parent) : QWidget(parent)
{
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);

  auto* frame = new QFrame(this);
  frame->setObjectName("inputsNotReadyFrame");
  frame->setStyleSheet(
    "QFrame#inputsNotReadyFrame { "
    "background: #fef3c7; border: 1px solid #fcd34d; "
    "border-radius: 4px; }"
    "QFrame#inputsNotReadyFrame QLabel { color: #b45309; }");

  auto* layout = new QVBoxLayout(frame);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(8);

  auto* message = new QLabel(frame);
  message->setWordWrap(true);
  message->setAlignment(Qt::AlignCenter);
  message->setText(
    tr("This node's properties widget requires connected, current "
       "input data.\n\nPlease ensure all input ports are connected and "
       "upstream nodes have been executed."));
  layout->addWidget(message);

  auto* buttonRow = new QHBoxLayout;
  buttonRow->addStretch();
  m_runButton = new QPushButton(tr("Run Pipeline to Generate Inputs"), frame);
  connect(m_runButton, &QPushButton::clicked,
          this, &InputsNotReadyWidget::runRequested);
  buttonRow->addWidget(m_runButton);
  buttonRow->addStretch();
  layout->addLayout(buttonRow);

  outer->addWidget(frame);
  outer->addStretch();
}

void InputsNotReadyWidget::setRunEnabled(bool enabled)
{
  m_runButton->setEnabled(enabled);
}

} // namespace pipeline
} // namespace tomviz
