/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Reaction.h"

#include "ActiveObjects.h"
#include "pipeline/OutputPort.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>

namespace tomviz {

Reaction::Reaction(QAction* parentObject) : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(),
          &ActiveObjects::activePipelineChanged,
          this, &Reaction::updateEnableState);
  connect(&ActiveObjects::instance(),
          &ActiveObjects::activeTipOutputPortChanged,
          this, &Reaction::updateEnableState);

  qApp->installEventFilter(this);
  updateEnableState();
}

bool Reaction::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::KeyPress ||
      event->type() == QEvent::KeyRelease) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Control) {
      m_ctrlHeld = (event->type() == QEvent::KeyPress);
      updateEnableState();
    }
  }
  return pqReaction::eventFilter(obj, event);
}

void Reaction::updateEnableState()
{
  if (m_ctrlHeld) {
    parentAction()->setEnabled(true);
    return;
  }

  auto& ao = ActiveObjects::instance();
  auto* tipPort = ao.activeTipOutputPort();
  if (!tipPort) {
    parentAction()->setEnabled(false);
    return;
  }
  parentAction()->setEnabled(
    pipeline::isPortTypeCompatible(tipPort->type(), m_acceptedInputTypes));
}

void Reaction::setAcceptedInputTypes(pipeline::PortTypes types)
{
  m_acceptedInputTypes = types;
  updateEnableState();
}

} // namespace tomviz
