/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SetTiltAnglesReaction.h"

#include "ActiveObjects.h"
#include "TransformUtils.h"

#include "pipeline/OutputPort.h"
#include "pipeline/transforms/SetTiltAnglesTransform.h"

#include <QMainWindow>

namespace tomviz {

SetTiltAnglesReaction::SetTiltAnglesReaction(QAction* p, QMainWindow* mw)
  : pqReaction(p), m_mainWindow(mw)
{
  connect(&ActiveObjects::instance(),
          &ActiveObjects::activePipelineChanged,
          this, [this]() { updateEnableState(); });
  connect(&ActiveObjects::instance(),
          &ActiveObjects::activeTipOutputPortChanged,
          this, [this]() { updateEnableState(); });
  updateEnableState();
}

void SetTiltAnglesReaction::updateEnableState()
{
  auto& ao = ActiveObjects::instance();
  auto* tipPort = ao.activeTipOutputPort();
  parentAction()->setEnabled(tipPort != nullptr);
}

void SetTiltAnglesReaction::showSetTiltAnglesUI(QMainWindow*, DataSource*)
{
  auto* transform = new pipeline::SetTiltAnglesTransform();
  insertTransformIntoPipeline(transform);
}
} // namespace tomviz
