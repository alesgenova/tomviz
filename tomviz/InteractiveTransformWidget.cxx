/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "InteractiveTransformWidget.h"

#include <pqView.h>

#include <vtkActor.h>
#include <vtkBoxWidget2.h>
#include <vtkCommand.h>
#include <vtkCustomBoxRepresentation.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSMViewProxy.h>
#include <vtkTransform.h>

namespace tomviz {

InteractiveTransformWidget& InteractiveTransformWidget::instance()
{
  static InteractiveTransformWidget theInstance;
  return theInstance;
}

InteractiveTransformWidget::InteractiveTransformWidget() : QObject(nullptr)
{
  m_boxRep->SetPlaceFactor(1.0);
  m_boxRep->HandlesOn();
  m_boxRep->SetHandleSize(10);

  m_boxWidget->SetRepresentation(m_boxRep.GetPointer());
  m_boxWidget->SetPriority(1);

  m_eventLink->Connect(m_boxWidget.GetPointer(), vtkCommand::InteractionEvent,
                       this, SLOT(onInteraction(vtkObject*)));
  m_eventLink->Connect(m_boxWidget.GetPointer(),
                       vtkCommand::EndInteractionEvent, this,
                       SLOT(onInteraction(vtkObject*)));
}

InteractiveTransformWidget::~InteractiveTransformWidget() {}

bool InteractiveTransformWidget::acquire(QObject* user)
{
  if (!user) {
    return false;
  }

  if (m_currentUser && m_currentUser != user) {
    return false;
  }

  if (m_currentUser == user) {
    return true;
  }

  m_currentUser = user;
  connect(user, &QObject::destroyed, this,
          &InteractiveTransformWidget::onUserDestroyed);
  return true;
}

void InteractiveTransformWidget::release(QObject* user)
{
  if (m_currentUser != user) {
    return;
  }

  disconnect(m_currentUser, &QObject::destroyed, this,
             &InteractiveTransformWidget::onUserDestroyed);

  m_currentUser = nullptr;
  m_translationEnabled = false;
  m_rotationEnabled = false;
  m_scalingEnabled = false;
  disableWidget();
  emit widgetReleased();
}

void InteractiveTransformWidget::onUserDestroyed()
{
  m_currentUser = nullptr;
  m_translationEnabled = false;
  m_rotationEnabled = false;
  m_scalingEnabled = false;
  disableWidget();
  emit widgetReleased();
}

void InteractiveTransformWidget::setView(pqView* view)
{
  if (m_view == view) {
    return;
  }

  if (view && view->getViewProxy() &&
      view->getViewProxy()->GetRenderWindow()) {
    m_boxWidget->SetInteractor(
      view->getViewProxy()->GetRenderWindow()->GetInteractor());
  } else {
    m_boxWidget->SetInteractor(nullptr);
    m_boxWidget->EnabledOff();
  }

  render();
  m_view = view;
  updateWidgetState();
}

void InteractiveTransformWidget::setTranslationEnabled(bool enabled)
{
  m_translationEnabled = enabled;
  updateWidgetState();
}

void InteractiveTransformWidget::setRotationEnabled(bool enabled)
{
  m_rotationEnabled = enabled;
  updateWidgetState();
}

void InteractiveTransformWidget::setScalingEnabled(bool enabled)
{
  m_scalingEnabled = enabled;
  updateWidgetState();
}

void InteractiveTransformWidget::setBounds(const double bounds[6])
{
  m_boxRep->PlaceWidget(const_cast<double*>(bounds));
  render();
}

void InteractiveTransformWidget::setTransform(const double position[3],
                                              const double orientation[3],
                                              const double scale[3])
{
  vtkNew<vtkTransform> t;
  t->Identity();
  t->Translate(const_cast<double*>(position));
  // Rotate in Z-X-Y order (same as vtkProp3D).
  t->RotateZ(orientation[2]);
  t->RotateX(orientation[0]);
  t->RotateY(orientation[1]);
  t->Scale(const_cast<double*>(scale));

  m_boxRep->SetTransform(t);
  render();
}

void InteractiveTransformWidget::onInteraction(vtkObject*)
{
  vtkNew<vtkTransform> t;
  m_boxRep->GetTransform(t);

  // Copy to local arrays — the pointer-returning GetPosition/GetScale/
  // GetOrientation methods on vtkTransform share the same internal buffer,
  // so passing them directly as arguments is undefined behavior.
  double position[3], orientation[3], scale[3];
  t->GetPosition(position);
  t->GetOrientation(orientation);
  t->GetScale(scale);

  emit transformChanged(position, orientation, scale);
  render();
}

void InteractiveTransformWidget::updateWidgetState()
{
  auto* widget = m_boxWidget.Get();
  auto* rep = m_boxRep.Get();

  if (!m_currentUser || !m_view) {
    widget->EnabledOff();
    render();
    return;
  }

  bool anyEnabled =
    m_translationEnabled || m_rotationEnabled || m_scalingEnabled;

  widget->SetEnabled(anyEnabled);
  if (!anyEnabled) {
    render();
    return;
  }

  widget->SetTranslationEnabled(m_translationEnabled);
  widget->SetRotationEnabled(m_rotationEnabled);
  widget->SetScalingEnabled(m_scalingEnabled);
  widget->SetMoveFacesEnabled(m_scalingEnabled);

  for (int i = 0; i < 6; ++i) {
    rep->GetHandle()[i]->SetVisibility(m_scalingEnabled);
  }
  rep->GetHandle()[6]->SetVisibility(m_translationEnabled);

  render();
}

void InteractiveTransformWidget::disableWidget()
{
  m_boxWidget->EnabledOff();
  render();
}

void InteractiveTransformWidget::render()
{
  if (!m_view) {
    return;
  }
  m_view->render();
}

} // namespace tomviz
