/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AxesReaction.h"

#include <ActiveObjects.h>

#include <pqRenderView.h>
#include <pqRenderViewSelectionReaction.h>
#include <vtkSMRenderViewProxy.h>

#include <QToolBar>

namespace tomviz {

AxesReaction::AxesReaction(QAction* parentObject, AxesReaction::Mode mode)
  : pqReaction(parentObject)
{
  m_reactionMode = mode;

  QObject::connect(
    &ActiveObjects::instance(),
    QOverload<vtkSMViewProxy*>::of(&ActiveObjects::viewChanged), this,
    &AxesReaction::updateEnableState, Qt::QueuedConnection);

  switch (m_reactionMode) {
    case SHOW_ORIENTATION_AXES:
      QObject::connect(parentObject, &QAction::toggled, this,
                       &AxesReaction::showOrientationAxes);
      break;
    case SHOW_CENTER_AXES:
      QObject::connect(parentObject, &QAction::toggled, this,
                       &AxesReaction::showCenterAxes);
      break;
    case PICK_CENTER: {
      auto selectionReaction = new pqRenderViewSelectionReaction(
        parentObject, nullptr,
        pqRenderViewSelectionReaction::SELECT_CUSTOM_BOX);
      QObject::connect(
        selectionReaction,
        QOverload<int, int, int, int>::of(
          &pqRenderViewSelectionReaction::selectedCustomBox),
        this, &AxesReaction::pickCenterOfRotation);
    } break;
    default:
      break;
  }

  updateEnableState();
}

void AxesReaction::onTriggered()
{
  switch (m_reactionMode) {
    case RESET_CENTER:
      resetCenterOfRotationToCenterOfCurrentData();
      break;
    default:
      break;
  }
}

void AxesReaction::updateEnableState()
{
  pqRenderView* renderView = ActiveObjects::instance().activePqRenderView();

  switch (m_reactionMode) {
    case SHOW_ORIENTATION_AXES:
      parentAction()->setEnabled(renderView != NULL);
      parentAction()->blockSignals(true);
      parentAction()->setChecked(
        renderView ? renderView->getOrientationAxesVisibility() : false);
      parentAction()->blockSignals(false);
      break;
    case SHOW_CENTER_AXES:
      parentAction()->setEnabled(renderView != NULL);
      parentAction()->blockSignals(true);
      parentAction()->setChecked(
        renderView ? renderView->getCenterAxesVisibility() : false);
      parentAction()->blockSignals(false);
      break;
    case RESET_CENTER:
      // TODO: re-enable when new pipeline provides active data bounds
      parentAction()->setEnabled(false);
      break;
    default:
      break;
  }
}

void AxesReaction::showOrientationAxes(bool show_axes)
{
  pqRenderView* renderView = ActiveObjects::instance().activePqRenderView();

  if (!renderView)
    return;

  renderView->setOrientationAxesVisibility(show_axes);
  renderView->render();
}

void AxesReaction::showCenterAxes(bool show_axes)
{
  pqRenderView* renderView = ActiveObjects::instance().activePqRenderView();

  if (!renderView)
    return;

  renderView->setCenterAxesVisibility(show_axes);
  renderView->render();
}

void AxesReaction::resetCenterOfRotationToCenterOfCurrentData()
{
  // TODO: re-implement using new pipeline to get active data bounds
}

void AxesReaction::pickCenterOfRotation(int posx, int posy)
{
  pqRenderView* renderView = ActiveObjects::instance().activePqRenderView();
  if (renderView) {
    int posxy[2] = { posx, posy };
    double center[3];
    double normal[3];

    vtkSMRenderViewProxy* proxy = renderView->getRenderViewProxy();
    // This function is supposed to pick a point on a surface. It will
    // return false if it did not find a surface, but it still moves the
    // center to where we want it to be. So use it anyways.
    proxy->ConvertDisplayToPointOnSurface(posxy, center, normal);
    renderView->setCenterOfRotation(center);
    renderView->render();
  }
}

void AxesReaction::addAllActionsToToolBar(QToolBar* toolBar)
{
  QAction* showOrientationAxesAction =
    toolBar->addAction(QIcon(":pqWidgets/Icons/pqShowOrientationAxes.svg"),
                       "Show Orientation Axes");
  showOrientationAxesAction->setCheckable(true);
  new AxesReaction(showOrientationAxesAction,
                   AxesReaction::SHOW_ORIENTATION_AXES);
  QAction* showCenterAxesAction = toolBar->addAction(
    QIcon(":pqWidgets/Icons/pqShowCenterAxes.svg"), "Show Center Axes");
  showCenterAxesAction->setCheckable(true);
  new AxesReaction(showCenterAxesAction, AxesReaction::SHOW_CENTER_AXES);
  QAction* resetCenterAction = toolBar->addAction(
    QIcon(":pqWidgets/Icons/pqResetCenter.svg"), "Reset Center");
  new AxesReaction(resetCenterAction, AxesReaction::RESET_CENTER);
  QAction* pickCenterAction = toolBar->addAction(
    QIcon(":pqWidgets/Icons/pqPickCenter.svg"), "Pick Center");
  pickCenterAction->setCheckable(true);
  new AxesReaction(pickCenterAction, AxesReaction::PICK_CENTER);
}

} // namespace tomviz
