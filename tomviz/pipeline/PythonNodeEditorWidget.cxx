/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonNodeEditorWidget.h"

#include "CustomPythonNodeWidget.h"
#include "ExternalNodeExecutor.h"
#include "NodePropertiesWidget.h"

#include <pqPythonSyntaxHighlighter.h>

#include <QScrollBar>

#include <QComboBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace tomviz {
namespace pipeline {

PythonNodeEditorWidget::PythonNodeEditorWidget(
  const QString& label, const QString& script, const QString& jsonDescription,
  const QMap<QString, QVariant>& currentValues, const QString& executorType,
  const QString& executorEnvPath,
  CustomPythonNodeWidget* customParamsWidget, QWidget* parent)
  : EditNodeWidget(parent), m_customParamsWidget(customParamsWidget)
{
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Name row
  auto* nameRow = new QWidget(this);
  auto* nameLayout = new QHBoxLayout(nameRow);
  nameLayout->setContentsMargins(5, 5, 5, 5);
  nameLayout->setSpacing(5);
  nameLayout->addWidget(new QLabel(tr("Name"), nameRow));
  m_nameEdit = new QLineEdit(label, nameRow);
  nameLayout->addWidget(m_nameEdit);
  mainLayout->addWidget(nameRow);

  // Tab widget
  m_tabWidget = new QTabWidget(this);
  mainLayout->addWidget(m_tabWidget, 1);

  // --- Tab 1: Script ---
  auto* scriptTab = new QWidget(m_tabWidget);
  auto* scriptLayout = new QVBoxLayout(scriptTab);

  m_scriptEdit = new QTextEdit(scriptTab);
  m_scriptEdit->setLineWrapMode(QTextEdit::NoWrap);

  auto* highlighter =
    new pqPythonSyntaxHighlighter(m_scriptEdit, *m_scriptEdit);

  // Set font after the highlighter ctor, which sets QFont("Monospace")
  m_scriptEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  // Wire up highlighting ourselves instead of ConnectHighligter(), which has
  // scroll-restoration and double-highlight bugs in the upstream ParaView code.
  connect(m_scriptEdit, &QTextEdit::textChanged, m_scriptEdit,
    [highlighter, edit = m_scriptEdit]() {
      const QString text = edit->toPlainText();
      const int cursorPos = edit->textCursor().position();
      const QString html = highlighter->Highlight(text);
      if (!html.isEmpty()) {
        const int vScroll = edit->verticalScrollBar()->value();
        const int hScroll = edit->horizontalScrollBar()->value();
        const bool blocked = edit->blockSignals(true);
        edit->setHtml(html);
        QTextCursor cursor = edit->textCursor();
        cursor.setPosition(cursorPos);
        edit->setTextCursor(cursor);
        edit->verticalScrollBar()->setValue(vScroll);
        edit->horizontalScrollBar()->setValue(hScroll);
        edit->blockSignals(blocked);
      }
    });

  if (!script.isEmpty()) {
    m_scriptEdit->setPlainText(script);
  }

  scriptLayout->addWidget(m_scriptEdit, 1);
  m_tabWidget->addTab(scriptTab, tr("Script"));

  // --- Tab 2: Parameters ---
  auto* paramsTab = new QWidget(m_tabWidget);
  auto* paramsLayout = new QVBoxLayout(paramsTab);

  if (m_customParamsWidget) {
    // Use the custom widget instead of auto-generated parameters
    m_customParamsWidget->setParent(paramsTab);
    paramsLayout->addWidget(m_customParamsWidget, 1);
  } else {
    // Show operator description from JSON if available
    if (!jsonDescription.isEmpty()) {
      QJsonDocument doc = QJsonDocument::fromJson(jsonDescription.toUtf8());
      if (doc.isObject()) {
        QString desc = doc.object().value("description").toString();
        if (!desc.isEmpty()) {
          auto* descLabel = new QLabel(desc, paramsTab);
          descLabel->setWordWrap(true);
          descLabel->setStyleSheet(
            "QLabel { color: palette(text); padding: 4px; }");
          paramsLayout->addWidget(descLabel);
        }
      }

      m_paramsWidget = new NodePropertiesWidget(jsonDescription,
                                                currentValues, paramsTab);
      paramsLayout->addWidget(m_paramsWidget, 1);
    }

    paramsLayout->addStretch();
  }

  m_tabWidget->addTab(paramsTab, tr("Parameters"));

  // --- Tab 3: Execution ---
  // Picks the per-node executor strategy. Empty string == Internal.
  auto* execTab = new QWidget(m_tabWidget);
  auto* execLayout = new QVBoxLayout(execTab);

  auto* execGridContainer = new QWidget(execTab);
  auto* execGrid = new QGridLayout(execGridContainer);
  execGrid->setContentsMargins(0, 0, 0, 0);

  auto* executorLabel = new QLabel(tr("Executor"), execGridContainer);
  m_executorCombo = new QComboBox(execGridContainer);
  m_executorCombo->addItem(tr("Internal"), QString());
  m_executorCombo->addItem(
    tr("External"), ExternalNodeExecutor::typeString());
  executorLabel->setBuddy(m_executorCombo);
  execGrid->addWidget(executorLabel, 0, 0);
  execGrid->addWidget(m_executorCombo, 0, 1);

  m_envPathLabel = new QLabel(tr("Python Env"), execGridContainer);
  m_envPathRow = new QWidget(execGridContainer);
  auto* envLayout = new QHBoxLayout(m_envPathRow);
  envLayout->setContentsMargins(0, 0, 0, 0);
  m_envPathEdit = new QLineEdit(executorEnvPath, m_envPathRow);
  m_envPathEdit->setPlaceholderText(
    tr("Path to a Python env containing tomviz-pipeline"));
  envLayout->addWidget(m_envPathEdit, 1);
  auto* browseBtn = new QPushButton(tr("Browse"), m_envPathRow);
  envLayout->addWidget(browseBtn);
  m_envPathLabel->setBuddy(m_envPathRow);
  execGrid->addWidget(m_envPathLabel, 1, 0);
  execGrid->addWidget(m_envPathRow, 1, 1);

  execLayout->addWidget(execGridContainer);
  execLayout->addStretch();
  m_tabWidget->addTab(execTab, tr("Execution"));

  int typeIdx = m_executorCombo->findData(executorType);
  if (typeIdx < 0) {
    typeIdx = 0;
  }
  m_executorCombo->setCurrentIndex(typeIdx);
  // Enable/disable rather than show/hide so the grid columns don't
  // resize when toggling between Internal and External.
  m_envPathLabel->setEnabled(!executorType.isEmpty());
  m_envPathRow->setEnabled(!executorType.isEmpty());

  connect(m_executorCombo, &QComboBox::currentIndexChanged, this,
          [this](int) {
            QString type = m_executorCombo->currentData().toString();
            m_envPathLabel->setEnabled(!type.isEmpty());
            m_envPathRow->setEnabled(!type.isEmpty());
          });
  connect(browseBtn, &QPushButton::clicked, this, [this]() {
    auto dir = QFileDialog::getExistingDirectory(
      this, tr("Select Python environment"), m_envPathEdit->text());
    if (!dir.isEmpty()) {
      m_envPathEdit->setText(dir);
    }
  });

  m_tabWidget->setCurrentIndex(1);
}

void PythonNodeEditorWidget::applyChangesToOperator()
{
  QMap<QString, QVariant> values;
  if (m_paramsWidget) {
    values = m_paramsWidget->values();
  }
  QString type = m_executorCombo->currentData().toString();
  QString envPath = type.isEmpty() ? QString() : m_envPathEdit->text();
  emit applied(m_nameEdit->text(), m_scriptEdit->toPlainText(), values, type,
               envPath);
}

void PythonNodeEditorWidget::showScriptTab()
{
  m_tabWidget->setCurrentIndex(0);
}

} // namespace pipeline
} // namespace tomviz
