/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PipelineControlsWidget.h"

#include "Pipeline.h"
#include "PipelineSettings.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include <QToolButton>

namespace tomviz {
namespace pipeline {

PipelineControlsWidget::PipelineControlsWidget(QWidget* parent)
  : QWidget(parent)
{
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(4, 2, 4, 2);

  m_spinnerLabel = new QLabel(this);
  m_spinnerLabel->setFixedSize(SpinnerSize, SpinnerSize);
  m_spinnerLabel->hide();
  layout->addWidget(m_spinnerLabel);

  m_statusLabel = new QLabel(this);
  QPalette labelPal = m_statusLabel->palette();
  labelPal.setColor(QPalette::WindowText, palette().color(QPalette::Dark));
  m_statusLabel->setPalette(labelPal);
  layout->addWidget(m_statusLabel);

  layout->addStretch();

  m_button = new QToolButton(this);
  m_button->setAutoRaise(true);
  m_button->setIconSize(QSize(20, 20));
  connect(m_button, &QToolButton::clicked, this,
          &PipelineControlsWidget::onButtonClicked);
  layout->addWidget(m_button);

  auto* separator = new QFrame(this);
  separator->setFrameShape(QFrame::VLine);
  separator->setFrameShadow(QFrame::Sunken);
  layout->addWidget(separator);

  m_dimmingButton = new QToolButton(this);
  m_dimmingButton->setAutoRaise(true);
  m_dimmingButton->setIconSize(QSize(20, 20));
  m_dimmingButton->setIcon(QIcon(":/icons/filter.svg"));
  m_dimmingButton->setToolTip(
    tr("Enable focus dimming (dim unrelated pipeline elements on selection)"));
  connect(m_dimmingButton, &QToolButton::clicked, this, [this]() {
    m_dimmingEnabled = !m_dimmingEnabled;
    if (m_dimmingEnabled) {
      m_dimmingButton->setIcon(QIcon(":/icons/filter_disabled.svg"));
      m_dimmingButton->setToolTip(
        tr("Disable focus dimming (show all pipeline elements at full opacity)"));
    } else {
      m_dimmingButton->setIcon(QIcon(":/icons/filter.svg"));
      m_dimmingButton->setToolTip(
        tr("Enable focus dimming (dim unrelated pipeline elements on selection)"));
    }
    emit dimmingToggled(m_dimmingEnabled);
  });
  layout->addWidget(m_dimmingButton);

  auto* persistenceSeparator = new QFrame(this);
  persistenceSeparator->setFrameShape(QFrame::VLine);
  persistenceSeparator->setFrameShadow(QFrame::Sunken);
  layout->addWidget(persistenceSeparator);

  setupPersistenceButton(layout);

  m_spinnerTimer = new QTimer(this);
  m_spinnerTimer->setInterval(50);
  connect(m_spinnerTimer, &QTimer::timeout, this, [this]() {
    m_spinnerAngle = (m_spinnerAngle + 30) % 360;
    // Repaint the spinner label
    QPixmap pix(SpinnerSize, SpinnerSize);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QPixmap spinner(QStringLiteral(":/pipeline/spinner.png"));
    p.translate(SpinnerSize / 2, SpinnerSize / 2);
    p.rotate(m_spinnerAngle);
    p.drawPixmap(-SpinnerSize / 2, -SpinnerSize / 2,
                 SpinnerSize, SpinnerSize, spinner);
    p.end();
    m_spinnerLabel->setPixmap(pix);
  });

  updateState();
}

void PipelineControlsWidget::setPipeline(Pipeline* pipeline)
{
  if (m_pipeline) {
    m_pipeline->disconnect(this);
  }
  m_pipeline = pipeline;
  if (m_pipeline) {
    connect(m_pipeline, &Pipeline::executionStarted, this,
            &PipelineControlsWidget::updateState);
    connect(m_pipeline, &Pipeline::executionFinished, this,
            &PipelineControlsWidget::updateState);
    connect(m_pipeline, &Pipeline::pausedChanged, this,
            &PipelineControlsWidget::updateState);
  }
  updateState();
}

Pipeline* PipelineControlsWidget::pipeline() const
{
  return m_pipeline;
}

bool PipelineControlsWidget::isDimmingEnabled() const
{
  return m_dimmingEnabled;
}

void PipelineControlsWidget::updateState()
{
  bool executing = m_pipeline && m_pipeline->isExecuting();

  // Clear stopping flag when execution finishes.
  if (!executing) {
    m_stopping = false;
  }

  // Button icon
  if (executing) {
    m_button->setIcon(QIcon(":/icons/pqVcrStop.svg"));
    m_button->setToolTip(tr("Stop execution"));
  } else if (m_pipeline && m_pipeline->isPaused()) {
    m_button->setIcon(QIcon(":/pqWidgets/Icons/pqVcrPlay.svg"));
    m_button->setToolTip(tr("Resume automatic execution"));
  } else {
    m_button->setIcon(QIcon(":/pqWidgets/Icons/pqVcrPause.svg"));
    m_button->setToolTip(tr("Pause automatic execution"));
  }

  // Status message and spinner
  if (m_stopping && executing) {
    m_statusLabel->setText(tr("Stopping..."));
    m_statusLabel->show();
    if (!m_spinnerTimer->isActive()) {
      m_spinnerTimer->start();
    }
    m_spinnerLabel->show();
  } else if (executing) {
    m_statusLabel->hide();
    if (!m_spinnerTimer->isActive()) {
      m_spinnerTimer->start();
    }
    m_spinnerLabel->show();
  } else if (m_pipeline && m_pipeline->isPaused()) {
    m_statusLabel->setText(tr("Automatic execution paused"));
    m_statusLabel->show();
    m_spinnerTimer->stop();
    m_spinnerLabel->hide();
  } else {
    m_statusLabel->hide();
    m_spinnerTimer->stop();
    m_spinnerLabel->hide();
  }
}

void PipelineControlsWidget::onButtonClicked()
{
  if (!m_pipeline) {
    return;
  }
  if (m_pipeline->isExecuting()) {
    m_stopping = true;
    m_pipeline->cancelExecution();
    updateState();
  } else {
    m_pipeline->setPaused(!m_pipeline->isPaused());
  }
}

void PipelineControlsWidget::setupPersistenceButton(QHBoxLayout* layout)
{
  m_persistenceButton = new QToolButton(this);
  m_persistenceButton->setAutoRaise(true);
  m_persistenceButton->setIconSize(QSize(20, 20));
  m_persistenceButton->setPopupMode(QToolButton::InstantPopup);

  auto* menu = new QMenu(m_persistenceButton);
  auto addModeAction = [menu](const QString& iconPath,
                              const QString& label,
                              TransformPersistenceDefault mode) {
    auto* action = menu->addAction(QIcon(iconPath), label);
    action->setData(static_cast<int>(mode));
    return action;
  };
  addModeAction(QStringLiteral(":/pipeline/port_persistent_ram.svg"),
                tr("Persist in Memory"),
                TransformPersistenceDefault::InMemory);
  addModeAction(QStringLiteral(":/pipeline/port_persistent_disk.svg"),
                tr("Persist on Disk"),
                TransformPersistenceDefault::OnDisk);
  addModeAction(QStringLiteral(":/pipeline/port_transient.svg"),
                tr("Transient"), TransformPersistenceDefault::Transient);
  m_persistenceButton->setMenu(menu);
  layout->addWidget(m_persistenceButton);

  connect(menu, &QMenu::triggered, this, [](QAction* action) {
    PipelineSettings::instance().setTransformPersistenceDefault(
      static_cast<TransformPersistenceDefault>(action->data().toInt()));
  });

  // Keep the button face in sync with the current setting — both at
  // construction and if the value is changed elsewhere.
  syncPersistenceButton(
    PipelineSettings::instance().transformPersistenceDefault());
  connect(&PipelineSettings::instance(),
          &PipelineSettings::transformPersistenceDefaultChanged, this,
          &PipelineControlsWidget::syncPersistenceButton);
}

void PipelineControlsWidget::syncPersistenceButton(
  TransformPersistenceDefault mode)
{
  if (!m_persistenceButton) {
    return;
  }
  QString iconPath;
  QString tooltip;
  switch (mode) {
    case TransformPersistenceDefault::InMemory:
      iconPath = QStringLiteral(":/pipeline/port_persistent_ram.svg");
      tooltip = tr("Intermediate Data default: Persist in Memory");
      break;
    case TransformPersistenceDefault::OnDisk:
      iconPath = QStringLiteral(":/pipeline/port_persistent_disk.svg");
      tooltip = tr("Intermediate Data default: Persist on Disk");
      break;
    case TransformPersistenceDefault::Transient:
      iconPath = QStringLiteral(":/pipeline/port_transient.svg");
      tooltip = tr("Intermediate Data default: Transient");
      break;
  }
  m_persistenceButton->setIcon(QIcon(iconPath));
  m_persistenceButton->setToolTip(tooltip);
}

} // namespace pipeline
} // namespace tomviz
