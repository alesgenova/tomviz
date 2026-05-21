/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ProgressDialogManager.h"

#include "pipeline/Node.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineExecutor.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QStatusBar>
#include <QVBoxLayout>

namespace tomviz {

ProgressDialogManager::ProgressDialogManager(QMainWindow* mw)
  : QObject(mw), m_mainWindow(mw)
{}

ProgressDialogManager::~ProgressDialogManager() = default;

bool ProgressDialogManager::eventFilter(QObject* obj, QEvent* event)
{
  if (obj == m_progressDialog) {
    if (event->type() == QEvent::KeyPress) {
      auto* keyEvent = static_cast<QKeyEvent*>(event);
      if (keyEvent->key() == Qt::Key_Escape) {
        return true; // swallow
      }
    }
    if (event->type() == QEvent::Close) {
      return true; // swallow
    }
  }
  return QObject::eventFilter(obj, event);
}

void ProgressDialogManager::setPipeline(pipeline::Pipeline* pipeline)
{
  if (m_pipeline) {
    m_pipeline->disconnect(this);
    if (auto* exec = m_pipeline->executor()) {
      exec->disconnect(this);
    }
  }

  m_pipeline = pipeline;
  if (!m_pipeline) {
    return;
  }

  connectExecutor();
}

void ProgressDialogManager::connectExecutor()
{
  auto* exec = m_pipeline ? m_pipeline->executor() : nullptr;
  if (!exec) {
    return;
  }

  // Disconnect any previous executor.
  disconnect(exec, nullptr, this, nullptr);

  connect(exec, &pipeline::PipelineExecutor::nodeExecutionStarted, this,
          &ProgressDialogManager::onNodeExecutionStarted);
  connect(exec, &pipeline::PipelineExecutor::nodeExecutionFinished, this,
          &ProgressDialogManager::onNodeExecutionFinished);
}

void ProgressDialogManager::onNodeExecutionStarted(pipeline::Node* node)
{
  // Close any stale dialog from a previous node.
  if (m_progressDialog) {
    m_progressDialog->accept();
  }

  auto* dialog = new QDialog(m_mainWindow);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  // Title bar with no close/minimize/maximize buttons.
  dialog->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint |
                         Qt::WindowTitleHint);
  dialog->installEventFilter(this);
  m_progressDialog = dialog;

  auto* layout = new QVBoxLayout();
  auto* messageLabel = new QLabel(dialog);
  messageLabel->setWordWrap(true);

  QWidget* progressWidget = node->getCustomProgressWidget(dialog);
  if (!progressWidget) {
    auto* progressBar = new QProgressBar(dialog);
    progressBar->setMinimum(0);
    progressBar->setMaximum(node->totalProgressSteps());
    progressWidget = progressBar;

    connect(node, &pipeline::Node::progressStepChanged, progressBar,
            &QProgressBar::setValue);
    connect(node, &pipeline::Node::totalProgressStepsChanged, progressBar,
            &QProgressBar::setMaximum);
    connect(node, &pipeline::Node::progressMessageChanged, messageLabel,
            &QLabel::setText);
    connect(node, &pipeline::Node::progressMessageChanged, this,
            &ProgressDialogManager::showStatusBarMessage);
  }

  layout->addWidget(progressWidget);
  layout->addWidget(messageLabel);

  if (node->supportsCompletionMidExecution()) {
    auto* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, node,
            &pipeline::Node::completeExecution);
    connect(buttons, &QDialogButtonBox::accepted, buttons,
            [buttons, messageLabel]() {
              buttons->setEnabled(false);
              messageLabel->setText(
                QCoreApplication::translate("ProgressDialogManager",
                                            "Completion pending..."));
            });
    connect(buttons, &QDialogButtonBox::rejected, node,
            &pipeline::Node::cancelExecution);
    connect(buttons, &QDialogButtonBox::rejected, buttons,
            [buttons, messageLabel]() {
              buttons->setEnabled(false);
              messageLabel->setText(
                QCoreApplication::translate("ProgressDialogManager",
                                            "Cancel pending..."));
            });
  } else if (node->supportsCancelingMidExecution()) {
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel,
                                         Qt::Horizontal, dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, node,
            &pipeline::Node::cancelExecution);
    connect(buttons, &QDialogButtonBox::rejected, buttons,
            [buttons, messageLabel]() {
              buttons->setEnabled(false);
              messageLabel->setText(
                QCoreApplication::translate("ProgressDialogManager",
                                            "Cancel pending..."));
            });
  }

  dialog->setWindowTitle(QString("%1 Progress").arg(node->label()));
  dialog->setLayout(layout);
  dialog->adjustSize();
  dialog->resize(500, dialog->height());
  dialog->show();
  QCoreApplication::processEvents();
}

void ProgressDialogManager::onNodeExecutionFinished(pipeline::Node* node,
                                                    bool /*success*/)
{
  Q_UNUSED(node);
  if (m_progressDialog) {
    m_progressDialog->accept();
  }
}

void ProgressDialogManager::showStatusBarMessage(const QString& message)
{
  m_mainWindow->statusBar()->showMessage(message, 3000);
}

} // namespace tomviz
