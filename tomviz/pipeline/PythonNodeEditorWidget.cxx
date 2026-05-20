/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonNodeEditorWidget.h"

#include "CustomPythonNodeWidget.h"
#include "ExternalNodeExecutor.h"
#include "InputPort.h"
#include "InputsNotReadyWidget.h"
#include "Link.h"
#include "Node.h"
#include "NodePropertiesWidget.h"
#include "OutputPort.h"
#include "ParameterInterfaceBuilder.h"
#include "Pipeline.h"
#include "data/VolumeData.h"

#include <pqPythonSyntaxHighlighter.h>

#include <QTextBlock>
#include <QTimer>

#include <QComboBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

// Copy character formatting (syntax colors) from src into dst without
// modifying dst's text.  Walks blocks in parallel, skipping blank lines
// that only exist in one document (the HTML round-trip can lose them).
void applySyntaxFormatting(QTextDocument* dst, const QTextDocument& src)
{
  QTextCursor cursor(dst);
  cursor.beginEditBlock();

  QTextBlock dstBlock = dst->begin();
  QTextBlock srcBlock = src.begin();

  while (dstBlock.isValid() && srcBlock.isValid()) {
    if (dstBlock.text() == srcBlock.text()) {
      for (auto it = srcBlock.begin(); !it.atEnd(); ++it) {
        QTextFragment frag = it.fragment();
        int start =
          dstBlock.position() + frag.position() - srcBlock.position();
        cursor.setPosition(start);
        cursor.setPosition(start + frag.length(),
                           QTextCursor::KeepAnchor);
        cursor.setCharFormat(frag.charFormat());
      }
      dstBlock = dstBlock.next();
      srcBlock = srcBlock.next();
    } else if (dstBlock.text().isEmpty()) {
      dstBlock = dstBlock.next();
    } else if (srcBlock.text().isEmpty()) {
      srcBlock = srcBlock.next();
    } else {
      dstBlock = dstBlock.next();
      srcBlock = srcBlock.next();
    }
  }

  cursor.endEditBlock();
}

} // anonymous namespace

namespace tomviz {
namespace pipeline {

PythonNodeEditorWidget::PythonNodeEditorWidget(
  Node* node, Pipeline* pipeline,
  const QString& label, const QString& script, const QString& jsonDescription,
  const QMap<QString, QVariant>& currentValues, const QString& executorType,
  const QString& executorEnvPath, CustomWidgetFactory customWidgetFactory,
  bool customWidgetNeedsData, QWidget* parent)
  : EditNodeWidget(parent), m_node(node), m_pipeline(pipeline),
    m_customFactory(std::move(customWidgetFactory)),
    m_customWidgetNeedsData(customWidgetNeedsData),
    m_jsonDescription(jsonDescription),
    m_currentValues(currentValues)
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

  // Wire up highlighting ourselves instead of ConnectHighligter(), which
  // uses setHtml() to replace the whole document — that loses blank lines.
  // Instead we parse the HTML into a temp document and copy only the
  // character formatting (colors) into the real document via
  // applySyntaxFormatting(), leaving the text untouched.
  auto* rehighlightTimer = new QTimer(m_scriptEdit);
  rehighlightTimer->setSingleShot(true);
  rehighlightTimer->setInterval(0);

  connect(m_scriptEdit, &QTextEdit::textChanged, m_scriptEdit,
    [rehighlightTimer]() { rehighlightTimer->start(); });

  connect(rehighlightTimer, &QTimer::timeout, m_scriptEdit,
    [highlighter, edit = m_scriptEdit]() {
      const QString html = highlighter->Highlight(edit->toPlainText());
      if (html.isEmpty()) {
        return;
      }
      QTextDocument tempDoc;
      tempDoc.setHtml(html);
      const bool blocked = edit->blockSignals(true);
      applySyntaxFormatting(edit->document(), tempDoc);
      edit->blockSignals(blocked);
    });

  if (!script.isEmpty()) {
    m_scriptEdit->setPlainText(script);
  }

  scriptLayout->addWidget(m_scriptEdit, 1);
  m_tabWidget->addTab(scriptTab, tr("Script"));

  // --- Tab 2: Parameters ---
  m_paramsTab = new QWidget(m_tabWidget);
  m_paramsLayout = new QVBoxLayout(m_paramsTab);

  // Modes (custom widget vs JSON form, with or without data dependency):
  //   1) No custom widget factory  → auto-form from JSON description.
  //      A "select_scalars" parameter in the JSON makes the form depend
  //      on input data (we need the list of scalar arrays from the
  //      upstream VolumeData to populate the picker).
  //   2) Custom widget, no data needed (or data already available)
  //      → build the custom widget immediately.
  //   3) Either path needs data and inputs aren't in memory yet
  //      → install InputsNotReadyWidget; swap to the real widget on
  //        the first executionFinished that delivers the data.
  if (!m_customFactory) {
    if (!jsonDescription.isEmpty()) {
      QJsonDocument doc = QJsonDocument::fromJson(jsonDescription.toUtf8());
      if (doc.isObject()) {
        for (const auto& p : doc.object().value("parameters").toArray()) {
          if (p.toObject().value("type").toString() == "select_scalars") {
            m_jsonFormNeedsData = true;
            break;
          }
        }
      }
    }
  }

  if (!m_customFactory) {
    if (jsonDescription.isEmpty()) {
      m_paramsLayout->addStretch();
    } else if (!m_jsonFormNeedsData || inputsInMemory()) {
      installJsonFormWidget();
    } else {
      installNotReadyWidget();
    }
  } else if (!m_customWidgetNeedsData || inputsInMemory()) {
    installCustomWidget();
  } else {
    installNotReadyWidget();
  }

  if ((m_customFactory && m_customWidgetNeedsData) || m_jsonFormNeedsData) {
    connect(m_pipeline, &Pipeline::executionStarted, this, [this]() {
      if (m_notReadyWidget) {
        m_notReadyWidget->setRunEnabled(false);
      }
    });
    connect(m_pipeline, &Pipeline::executionFinished,
            this, &PythonNodeEditorWidget::onExecutionFinished);
  }

  m_tabWidget->addTab(m_paramsTab, tr("Parameters"));

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

bool PythonNodeEditorWidget::inputsInMemory() const
{
  for (auto* input : m_node->inputPorts()) {
    if (!input->link() || input->isStale() || !input->hasData()) {
      return false;
    }
    if (input->link()->from()->dataLocation() != DataLocation::InMemory) {
      return false;
    }
  }
  return true;
}

void PythonNodeEditorWidget::installCustomWidget()
{
  m_customParamsWidget = m_customFactory(m_paramsTab);
  if (!m_customParamsWidget) {
    return;
  }
  m_customParamsWidget->setValues(m_currentValues);
  m_paramsLayout->addWidget(m_customParamsWidget, 1);
}

void PythonNodeEditorWidget::installJsonFormWidget()
{
  QJsonDocument doc = QJsonDocument::fromJson(m_jsonDescription.toUtf8());
  if (doc.isObject()) {
    QString desc = doc.object().value("description").toString();
    if (!desc.isEmpty()) {
      auto* descLabel = new QLabel(desc, m_paramsTab);
      descLabel->setWordWrap(true);
      descLabel->setStyleSheet(
        "QLabel { color: palette(text); padding: 4px; }");
      m_paramsLayout->addWidget(descLabel);
    }
  }

  QList<PortScalars> portScalars;
  for (auto* input : m_node->inputPorts()) {
    if (!input->hasData()) {
      continue;
    }
    auto portData = input->data();
    if (!portData.isValid() || !isVolumeType(portData.type())) {
      continue;
    }
    auto vol = portData.value<VolumeDataPtr>();
    if (!vol || !vol->isValid()) {
      continue;
    }
    portScalars.append(
      { input->name(), vol->scalarNames(), vol->activeScalarName() });
  }

  m_paramsWidget = new NodePropertiesWidget(
    m_jsonDescription, m_currentValues, portScalars, m_paramsTab);
  m_paramsLayout->addWidget(m_paramsWidget, 1);
  m_paramsLayout->addStretch();
}

void PythonNodeEditorWidget::installNotReadyWidget()
{
  m_notReadyWidget = new InputsNotReadyWidget(m_paramsTab);
  m_notReadyWidget->setRunEnabled(!m_pipeline->isExecuting());
  connect(m_notReadyWidget, &InputsNotReadyWidget::runRequested,
          this, &PythonNodeEditorWidget::onRunRequested);
  m_paramsLayout->addWidget(m_notReadyWidget, 1);
}

void PythonNodeEditorWidget::onRunRequested()
{
  m_pipeline->executeUpstreamOf(m_node);
}

void PythonNodeEditorWidget::onExecutionFinished()
{
  if (!m_notReadyWidget) {
    return;
  }

  for (auto* input : m_node->inputPorts()) {
    auto* link = input->link();
    if (!link || !link->from()) {
      continue;
    }
    if (auto handle = link->from()->materialize()) {
      m_inputPins.append(handle);
    }
  }

  if (!inputsInMemory()) {
    m_notReadyWidget->setRunEnabled(true);
    return;
  }

  m_paramsLayout->removeWidget(m_notReadyWidget);
  m_notReadyWidget->deleteLater();
  m_notReadyWidget = nullptr;
  if (m_customFactory) {
    installCustomWidget();
  } else {
    installJsonFormWidget();
  }
}

void PythonNodeEditorWidget::applyChangesToOperator()
{
  QMap<QString, QVariant> values;
  if (m_customParamsWidget) {
    m_customParamsWidget->getValues(values);
    m_customParamsWidget->writeSettings();
  } else if (m_paramsWidget) {
    values = m_paramsWidget->values();
  }
  // If neither is set (Parameters tab is the not-ready warning), emit
  // an empty values map — the Python node leaves existing parameters
  // unchanged but still picks up Script and Execution edits.
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
