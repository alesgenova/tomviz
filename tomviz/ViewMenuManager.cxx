/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ViewMenuManager.h"

#include <pqCoreUtilities.h>
#include <pqRenderView.h>
#include <pqView.h>
#include <vtkPVRenderView.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMViewProxy.h>

#include <vtkCamera.h>
#include <vtkColorTransferFunction.h>
#include <vtkCommand.h>
#include <vtkGridAxesActor3D.h>
#include <vtkImageData.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkTextProperty.h>

#include <QAction>
#include <QActionGroup>
#include <QDebug>
#include <QDialog>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMainWindow>
#include <QMenu>

#include "ActiveObjects.h"
#include "CameraReaction.h"
#include "legacy/DataSource.h"
#include "pipeline/InputPort.h"
#include "pipeline/Link.h"
#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PortType.h"
#include "pipeline/SinkGroupNode.h"
#include "pipeline/SinkNode.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/sinks/LegacyModuleSink.h"
#include "pipeline/sinks/SliceSink.h"
#include "SliceViewDialog.h"
#include "Utilities.h"

namespace tomviz {

class PreviousImageViewerSettings
{
public:
  vtkNew<vtkCamera> camera;
  QString projection;
  bool newSliceSink = false;
  QPointer<pipeline::SliceSink> sliceSink;
  QJsonObject sliceSinkSettings;
  QList<QPointer<pipeline::LegacyModuleSink>> visibleSinks;
  int interactionMode = 0;

  void clear()
  {
    visibleSinks.clear();
    newSliceSink = false;
    sliceSink = nullptr;
    sliceSinkSettings = QJsonObject();
  };
};

ViewMenuManager::ViewMenuManager(QMainWindow* mainWindow, QMenu* menu)
  : pqViewMenuManager(mainWindow, menu), m_perspectiveProjectionAction(nullptr),
    m_orthographicProjectionAction(nullptr)
{
  m_view = ActiveObjects::instance().activeView();
  if (m_view) {
    m_viewObserverId =
      pqCoreUtilities::connect(m_view, vtkCommand::PropertyModifiedEvent, this,
                               SLOT(onViewPropertyChanged()));
  }
  connect(&ActiveObjects::instance(),
          static_cast<void (ActiveObjects::*)(vtkSMViewProxy*)>(
            &ActiveObjects::viewChanged),
          this, &ViewMenuManager::onViewChanged);

  connect(&ActiveObjects::instance(), &ActiveObjects::activeNodeChanged, this,
          [this]() {
            // TODO: extract DataSource from active node/port
            updateDataSource(nullptr);
          });
  connect(&ActiveObjects::instance(), &ActiveObjects::setImageViewerMode, this,
          &ViewMenuManager::setImageViewerMode);

  Menu->addSeparator();
  // Projection modes
  QActionGroup* projectionGroup = new QActionGroup(this);

  m_perspectiveProjectionAction = Menu->addAction("Perspective Projection");
  m_perspectiveProjectionAction->setCheckable(true);
  m_perspectiveProjectionAction->setActionGroup(projectionGroup);
  m_perspectiveProjectionAction->setChecked(true);
  connect(m_perspectiveProjectionAction, &QAction::triggered, this,
          &ViewMenuManager::setProjectionModeToPerspective);
  m_orthographicProjectionAction = Menu->addAction("Orthographic Projection");
  m_orthographicProjectionAction->setCheckable(true);
  m_orthographicProjectionAction->setActionGroup(projectionGroup);
  m_orthographicProjectionAction->setChecked(false);
  connect(m_orthographicProjectionAction, &QAction::triggered, this,
          &ViewMenuManager::setProjectionModeToOrthographic);

  Menu->addSeparator();

  m_showCenterAxesAction = Menu->addAction("Show Center Axes");
  m_showCenterAxesAction->setCheckable(true);
  m_showCenterAxesAction->setChecked(false);
  connect(m_showCenterAxesAction, &QAction::triggered, this,
          &ViewMenuManager::setShowCenterAxes);
  m_showOrientationAxesAction = Menu->addAction("Show Orientation Axes");
  m_showOrientationAxesAction->setCheckable(true);
  m_showOrientationAxesAction->setChecked(true);
  connect(m_showOrientationAxesAction, &QAction::triggered, this,
          &ViewMenuManager::setShowOrientationAxes);

  Menu->addSeparator();

  m_imageViewerModeAction = Menu->addAction("Image Viewer Mode");
  m_imageViewerModeAction->setCheckable(true);
  m_imageViewerModeAction->setChecked(false);
  connect(m_imageViewerModeAction, &QAction::triggered, this,
          &ViewMenuManager::setImageViewerMode);

  Menu->addSeparator();

  m_showDarkWhiteDataAction = Menu->addAction("Show Dark/White Data");
  m_showDarkWhiteDataAction->setEnabled(false);
  connect(m_showDarkWhiteDataAction, &QAction::triggered, this,
          &ViewMenuManager::showDarkWhiteData);

  Menu->addSeparator();

  m_previousImageViewerSettings.reset(new PreviousImageViewerSettings);

  if (hasLookingGlassPlugin()) {
    setupLookingGlassPlaceholder(mainWindow);
  }
}

ViewMenuManager::~ViewMenuManager()
{
  if (m_view) {
    m_view->RemoveObserver(m_viewObserverId);
  }
}

QString ViewMenuManager::projectionMode() const
{
  if (!m_view->GetProperty("CameraParallelProjection")) {
    return "";
  }
  int parallel =
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").GetAsInt();

  return parallel == 0 ? "Perspective" : "Orthographic";
}

void ViewMenuManager::setProjectionMode(QString mode)
{
  if (mode == "Perspective") {
    setProjectionModeToPerspective();
  } else if (mode == "Orthographic") {
    setProjectionModeToOrthographic();
  } else {
    qCritical() << "Invalid projection mode: " << mode;
  }
}

void ViewMenuManager::setProjectionModeToPerspective()
{
  if (!m_view->GetProperty("CameraParallelProjection")) {
    return;
  }
  int parallel =
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").GetAsInt();
  if (parallel) {
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").Set(0);
    m_view->UpdateVTKObjects();
    render();
  }
}

void ViewMenuManager::setProjectionModeToOrthographic()
{
  if (!m_view->GetProperty("CameraParallelProjection")) {
    return;
  }
  int parallel =
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").GetAsInt();
  if (!parallel) {
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").Set(1);
    m_view->UpdateVTKObjects();
    render();
  }
}

void ViewMenuManager::onViewPropertyChanged()
{
  if (!m_perspectiveProjectionAction || !m_orthographicProjectionAction) {
    return;
  }
  if (!m_view->GetProperty("CameraParallelProjection")) {
    return;
  }
  int parallel =
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").GetAsInt();
  if (parallel && m_perspectiveProjectionAction->isChecked()) {
    m_orthographicProjectionAction->setChecked(true);
  } else if (!parallel && m_orthographicProjectionAction->isChecked()) {
    m_perspectiveProjectionAction->setChecked(true);
  }
}

void ViewMenuManager::onViewChanged()
{
  if (m_view) {
    m_view->RemoveObserver(m_viewObserverId);
  }

  m_view = ActiveObjects::instance().activeView();

  if (m_view) {
    m_viewObserverId =
      pqCoreUtilities::connect(m_view, vtkCommand::PropertyModifiedEvent, this,
                               SLOT(onViewPropertyChanged()));
  }

  bool enableProjectionModes =
    (m_view && m_view->GetProperty("CameraParallelProjection"));
  m_orthographicProjectionAction->setEnabled(enableProjectionModes);
  m_perspectiveProjectionAction->setEnabled(enableProjectionModes);
  if (enableProjectionModes) {
    vtkSMPropertyHelper parallelProjection(m_view, "CameraParallelProjection");
    m_orthographicProjectionAction->setChecked(parallelProjection.GetAsInt() ==
                                               1);
    m_perspectiveProjectionAction->setChecked(parallelProjection.GetAsInt() !=
                                              1);
  }
  bool enableCenterAxes = m_view && m_view->GetProperty("CenterAxesVisibility");
  bool enableOrientationAxes =
    m_view && m_view->GetProperty("OrientationAxesVisibility");
  m_showCenterAxesAction->setEnabled(enableCenterAxes);
  m_showOrientationAxesAction->setEnabled(enableOrientationAxes);
  if (enableCenterAxes) {
    vtkSMPropertyHelper showCenterAxes(m_view, "CenterAxesVisibility");
    m_showCenterAxesAction->setChecked(showCenterAxes.GetAsInt() == 1);
  }
  if (enableOrientationAxes) {
    vtkSMPropertyHelper showOrientationAxes(m_view,
                                            "OrientationAxesVisibility");
    m_showOrientationAxesAction->setChecked(showOrientationAxes.GetAsInt() ==
                                            1);
  }
}

void ViewMenuManager::setShowCenterAxes(bool show)
{
  if (!m_view || !m_view->GetProperty("CenterAxesVisibility")) {
    return;
  }
  vtkSMPropertyHelper visibility(m_view, "CenterAxesVisibility");
  visibility.Set(show ? 1 : 0);
  m_view->UpdateVTKObjects();
  render();
}

void ViewMenuManager::setShowOrientationAxes(bool show)
{
  if (!m_view || !m_view->GetProperty("OrientationAxesVisibility")) {
    return;
  }
  vtkSMPropertyHelper visibility(m_view, "OrientationAxesVisibility");
  visibility.Set(show ? 1 : 0);
  m_view->UpdateVTKObjects();
  render();
}

int ViewMenuManager::interactionMode() const
{
  auto* renderView = ActiveObjects::instance().activePqRenderView();
  return vtkSMPropertyHelper(renderView->getProxy(), "InteractionMode")
    .GetAsInt();
}

void ViewMenuManager::setInteractionMode(int mode)
{
  auto* renderView = ActiveObjects::instance().activePqRenderView();
  vtkSMPropertyHelper(renderView->getProxy(), "InteractionMode").Set(mode);
  renderView->getProxy()->UpdateProperty("InteractionMode", 1);
}

void ViewMenuManager::render()
{
  auto* view = tomviz::convert<pqView*>(m_view);
  if (view) {
    view->render();
  }
}

static void resize2DCameraToFit(vtkSMRenderViewProxy* view, double bounds[6],
                                int axis)
{
  double lengths[3] = {
    bounds[1] - bounds[0], bounds[3] - bounds[2], bounds[5] - bounds[4],
  };

  int* size = view->GetRenderWindow()->GetSize();
  double w = size[0];
  double h = size[1];

  double bw, bh;
  if (axis == 0 || axis == 2) {
    bw = lengths[(axis + 1) % 3];
    bh = lengths[(axis + 2) % 3];
  } else {
    bw = lengths[(axis + 2) % 3];
    bh = lengths[(axis + 1) % 3];
  }
  double viewAspect = w / h;
  double boundsAspect = bw / bh;

  double scale = 0;
  if (viewAspect >= boundsAspect) {
    scale = bh / 2;
  } else {
    scale = bw / 2 / viewAspect;
  }

  auto* camera = view->GetActiveCamera();
  camera->SetParallelScale(scale);

  double depth = lengths[axis];
  if (depth < 1.0) {
    depth = 1.0;
  }
  double dist = camera->GetDistance();
  camera->SetClippingRange(dist - depth, dist + depth);
}

void ViewMenuManager::setImageViewerMode(bool enable)
{
  if (m_imageViewerModeAction->isChecked() != enable) {
    QSignalBlocker blocked(m_imageViewerModeAction);
    m_imageViewerModeAction->setChecked(enable);
  }

  if (!enable && enable == m_imageViewerMode) {
    return;
  }
  m_imageViewerMode = enable;

  if (!enable) {
    emit imageViewerModeToggled(enable);
    restoreImageViewerSettings();
    return;
  }

  auto* pip = ActiveObjects::instance().pipeline();
  auto* tipPort = ActiveObjects::instance().activeTipOutputPort();
  if (!pip || !tipPort) {
    return;
  }

  auto* view =
    vtkSMRenderViewProxy::SafeDownCast(ActiveObjects::instance().activeView());
  auto* camera = view->GetActiveCamera();

  auto& oldSettings = m_previousImageViewerSettings;
  oldSettings->clear();
  oldSettings->camera->ShallowCopy(camera);
  oldSettings->projection = projectionMode();
  oldSettings->interactionMode = interactionMode();

  setProjectionModeToOrthographic();
  setInteractionMode(vtkPVRenderView::INTERACTION_MODE_2D);

  // Find an existing SliceSink connected to the tip port
  pipeline::SliceSink* sliceSink = nullptr;
  for (auto* link : tipPort->links()) {
    auto* sg =
      qobject_cast<pipeline::SinkGroupNode*>(link->to()->node());
    if (!sg) {
      continue;
    }
    for (auto* sinkNode : sg->sinks()) {
      auto* candidate = qobject_cast<pipeline::SliceSink*>(sinkNode);
      if (candidate) {
        sliceSink = candidate;
        break;
      }
    }
    if (sliceSink) {
      break;
    }
  }

  oldSettings->newSliceSink = !sliceSink;
  if (sliceSink) {
    oldSettings->sliceSinkSettings = sliceSink->serialize();
    sliceSink->setVisibility(true);
  } else {
    sliceSink = new pipeline::SliceSink();
    sliceSink->setLabel("Slice");
    if (!sliceSink->initialize(view)) {
      delete sliceSink;
      m_imageViewerMode = false;
      QSignalBlocker blocked(m_imageViewerModeAction);
      m_imageViewerModeAction->setChecked(false);
      return;
    }
    pip->addNode(sliceSink);

    auto* input = sliceSink->inputPorts()[0];
    pipeline::OutputPort* connectTo = nullptr;
    for (auto* link : tipPort->links()) {
      auto* sg =
        qobject_cast<pipeline::SinkGroupNode*>(link->to()->node());
      if (sg) {
        int idx = sg->inputPorts().indexOf(link->to());
        if (idx >= 0 && idx < sg->outputPorts().size() &&
            pipeline::isPortTypeCompatible(sg->outputPorts()[idx]->type(),
                                           input->acceptedTypes())) {
          connectTo = sg->outputPorts()[idx];
          break;
        }
      }
    }
    if (!connectTo) {
      auto* group = new pipeline::SinkGroupNode();
      pipeline::PortType groupType =
        pipeline::isVolumeType(tipPort->type())
          ? pipeline::PortType::ImageData
          : tipPort->type();
      group->addPassthrough(tipPort->name(), groupType);
      pip->addNode(group);
      pip->createLink(tipPort, group->inputPorts()[0]);
      connectTo = group->outputPorts()[0];
    }
    pip->createLink(connectTo, input);
    pip->execute();
  }
  oldSettings->sliceSink = sliceSink;

  // Hide other sinks on the same port
  for (auto* link : tipPort->links()) {
    auto* sg =
      qobject_cast<pipeline::SinkGroupNode*>(link->to()->node());
    if (!sg) {
      continue;
    }
    for (auto* sinkNode : sg->sinks()) {
      auto* legacySink =
        qobject_cast<pipeline::LegacyModuleSink*>(sinkNode);
      if (legacySink && legacySink != sliceSink &&
          legacySink->visibility()) {
        oldSettings->visibleSinks.append(legacySink);
        legacySink->setVisibility(false);
      }
    }
  }

  if (oldSettings->newSliceSink) {
    sliceSink->setDirection(pipeline::SliceSink::XY);
    sliceSink->setSlice(0);
  }
  sliceSink->setShowArrow(false);

  int axis = 2;
  switch (sliceSink->direction()) {
    case pipeline::SliceSink::YZ: axis = 0; break;
    case pipeline::SliceSink::XZ: axis = 1; break;
    default: axis = 2; break;
  }

  switch (axis) {
    case 0: CameraReaction::resetPositiveX(); break;
    case 1: CameraReaction::resetPositiveY(); break;
    default: CameraReaction::resetNegativeZ(); break;
  }

  double bounds[6];
  auto vol = sliceSink->volumeData();
  if (vol && vol->isValid()) {
    vol->imageData()->GetBounds(bounds);
  } else {
    auto portData = tipPort->data();
    if (portData.isValid() && pipeline::isVolumeType(portData.type())) {
      auto tipVol = portData.value<pipeline::VolumeDataPtr>();
      if (tipVol && tipVol->isValid()) {
        tipVol->imageData()->GetBounds(bounds);
      }
    }
  }
  resize2DCameraToFit(view, bounds, axis);

  emit imageViewerModeToggled(enable);
  render();
}

void ViewMenuManager::restoreImageViewerSettings()
{
  auto& settings = m_previousImageViewerSettings;

  auto* view =
    vtkSMRenderViewProxy::SafeDownCast(ActiveObjects::instance().activeView());
  auto* camera = view->GetActiveCamera();

  setInteractionMode(settings->interactionMode);
  setProjectionMode(settings->projection);
  camera->ShallowCopy(settings->camera);

  if (settings->sliceSink) {
    if (settings->newSliceSink) {
      auto* pip = ActiveObjects::instance().pipeline();
      if (pip) {
        pip->removeNode(settings->sliceSink);
      }
    } else {
      settings->sliceSink->deserialize(settings->sliceSinkSettings);
    }
  }

  for (auto sink : settings->visibleSinks) {
    if (sink) {
      sink->setVisibility(true);
    }
  }

  view->ResetCamera();
  render();
}

void ViewMenuManager::updateDataSource(DataSource* s)
{
  m_dataSource = s;
  updateDataSourceEnableStates();
}

void ViewMenuManager::updateDataSourceEnableStates()
{
  // Currently, both white and dark are required to use this
  // We can change this in the future if needed...
  m_showDarkWhiteDataAction->setEnabled(
    m_dataSource && m_dataSource->darkData() && m_dataSource->whiteData());
}

void ViewMenuManager::showDarkWhiteData()
{
  if (!m_dataSource || !m_dataSource->darkData() ||
      !m_dataSource->whiteData()) {
    return;
  }

  if (!m_sliceViewDialog) {
    m_sliceViewDialog.reset(new SliceViewDialog);
  }

  auto* lut = vtkColorTransferFunction::SafeDownCast(
    m_dataSource->colorMap()->GetClientSideObject());

  m_sliceViewDialog->setLookupTable(lut);
  m_sliceViewDialog->setDarkImage(m_dataSource->darkData());
  m_sliceViewDialog->setWhiteImage(m_dataSource->whiteData());
  m_sliceViewDialog->switchToDark();

  m_sliceViewDialog->exec();
}

void ViewMenuManager::setupLookingGlassPlaceholder(QMainWindow* mainWindow)
{
  // Add the Looking Glass menu item placeholder, which, when checked,
  // will cause the plugin to load, and the placeholder will remove itself.
  // This needs to be done so that the EULA will only pop up if the user
  // tries to use the looking glass plugin.
  // We're gonna take advantage of the fact that
  // pqViewMenuManager::updateMenu() automatically adds entries for dock
  // widgets in order. We will make a fake dock widget, which will get added,
  // then when that action is triggered, we will load the plugin and replace
  // the fake action with the real one.

  // Create the fake dock widget
  QPointer<QDockWidget> lgPlaceholderWidget = new QDockWidget(mainWindow);
  lgPlaceholderWidget->setVisible(false);

  // Get the action
  auto* lgPlaceholderAction = lgPlaceholderWidget->toggleViewAction();
  lgPlaceholderAction->setText("Looking Glass");

  // This will place the dock widget action in the menu
  this->updateMenu();

  // If the action is triggered, load the plugin and remove the placeholder
  connect(lgPlaceholderAction, &QAction::triggered, lgPlaceholderWidget,
          [lgPlaceholderWidget]() {
            loadLookingGlassPlugin();
            lgPlaceholderWidget->deleteLater();
          });
}

} // namespace tomviz
