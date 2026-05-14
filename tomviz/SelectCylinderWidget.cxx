/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SelectCylinderWidget.h"

#include "ActiveObjects.h"
#include "pipeline/data/VolumeData.h"

#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkImageData.h>
#include <vtkImplicitCylinderRepresentation.h>
#include <vtkImplicitCylinderWidget.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkWidgetCallbackMapper.h>
#include <vtkWidgetEvent.h>
#include <vtkWidgetEventTranslator.h>

#include <vtkSMViewProxy.h>

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

// Subclass that forces left-click = translate center, right-click = adjust radius.
class CylinderWidgetCustom : public vtkImplicitCylinderWidget
{
public:
  static CylinderWidgetCustom* New()
  {
    auto* self = new CylinderWidgetCustom;
    self->InitializeObjectBase();
    return self;
  }
  vtkTypeMacro(CylinderWidgetCustom, vtkImplicitCylinderWidget);

  void setupInteraction()
  {
    this->EventTranslator->ClearEvents();
    this->EventTranslator->SetTranslation(vtkCommand::LeftButtonPressEvent,
                                           vtkWidgetEvent::Translate);
    this->EventTranslator->SetTranslation(vtkCommand::LeftButtonReleaseEvent,
                                           vtkWidgetEvent::EndTranslate);
    this->EventTranslator->SetTranslation(vtkCommand::RightButtonPressEvent,
                                           vtkWidgetEvent::Scale);
    this->EventTranslator->SetTranslation(vtkCommand::RightButtonReleaseEvent,
                                           vtkWidgetEvent::EndScale);
    this->EventTranslator->SetTranslation(vtkCommand::MouseMoveEvent,
                                           vtkWidgetEvent::Move);

    this->CallbackMapper->SetCallbackMethod(
      vtkCommand::LeftButtonPressEvent, vtkWidgetEvent::Translate, this,
      CylinderWidgetCustom::ForceTranslateAction);
    this->CallbackMapper->SetCallbackMethod(
      vtkCommand::LeftButtonReleaseEvent, vtkWidgetEvent::EndTranslate, this,
      vtkImplicitCylinderWidget::EndSelectAction);
    this->CallbackMapper->SetCallbackMethod(
      vtkCommand::RightButtonPressEvent, vtkWidgetEvent::Scale, this,
      CylinderWidgetCustom::ForceRadiusAction);
    this->CallbackMapper->SetCallbackMethod(
      vtkCommand::RightButtonReleaseEvent, vtkWidgetEvent::EndScale, this,
      vtkImplicitCylinderWidget::EndSelectAction);
    this->CallbackMapper->SetCallbackMethod(
      vtkCommand::MouseMoveEvent, vtkWidgetEvent::Move, this,
      vtkImplicitCylinderWidget::MoveAction);
  }

  static void ForceTranslateAction(vtkAbstractWidget* w)
  {
    auto* self = reinterpret_cast<CylinderWidgetCustom*>(w);
    int X = self->Interactor->GetEventPosition()[0];
    int Y = self->Interactor->GetEventPosition()[1];

    auto* rep =
      reinterpret_cast<vtkImplicitCylinderRepresentation*>(self->WidgetRep);
    rep->SetInteractionState(vtkImplicitCylinderRepresentation::Moving);
    int state = rep->ComputeInteractionState(X, Y);

    if (state == vtkImplicitCylinderRepresentation::Outside) {
      return;
    }

    // If the axis arrow was picked, allow rotation; otherwise force translate
    if (state != vtkImplicitCylinderRepresentation::RotatingAxis) {
      rep->SetInteractionState(vtkImplicitCylinderRepresentation::MovingCenter);
    }

    self->GrabFocus(self->EventCallbackCommand);
    double eventPos[2] = { static_cast<double>(X), static_cast<double>(Y) };
    self->WidgetState = vtkImplicitCylinderWidget::Active;
    self->WidgetRep->StartWidgetInteraction(eventPos);

    self->EventCallbackCommand->SetAbortFlag(1);
    self->StartInteraction();
    self->InvokeEvent(vtkCommand::StartInteractionEvent, nullptr);
    self->Render();
  }

  static void ForceRadiusAction(vtkAbstractWidget* w)
  {
    auto* self = reinterpret_cast<CylinderWidgetCustom*>(w);
    int X = self->Interactor->GetEventPosition()[0];
    int Y = self->Interactor->GetEventPosition()[1];

    auto* rep =
      reinterpret_cast<vtkImplicitCylinderRepresentation*>(self->WidgetRep);
    rep->SetInteractionState(vtkImplicitCylinderRepresentation::Moving);
    rep->ComputeInteractionState(X, Y);

    if (rep->GetInteractionState() ==
        vtkImplicitCylinderRepresentation::Outside) {
      return;
    }

    // Force radius adjustment regardless of what was picked
    rep->SetInteractionState(
      vtkImplicitCylinderRepresentation::AdjustingRadius);

    self->GrabFocus(self->EventCallbackCommand);
    double eventPos[2] = { static_cast<double>(X), static_cast<double>(Y) };
    self->WidgetState = vtkImplicitCylinderWidget::Active;
    self->WidgetRep->StartWidgetInteraction(eventPos);

    self->EventCallbackCommand->SetAbortFlag(1);
    self->StartInteraction();
    self->InvokeEvent(vtkCommand::StartInteractionEvent, nullptr);
    self->Render();
  }
};

} // anonymous namespace

namespace tomviz {

class SelectCylinderWidget::Internal
{
public:
  vtkNew<CylinderWidgetCustom> cylinderWidget;
  vtkNew<vtkEventQtSlotConnect> eventLink;
  vtkSmartPointer<vtkRenderWindowInteractor> interactor;

  QDoubleSpinBox* centerX = nullptr;
  QDoubleSpinBox* centerY = nullptr;
  QDoubleSpinBox* centerZ = nullptr;
  QDoubleSpinBox* axisX = nullptr;
  QDoubleSpinBox* axisY = nullptr;
  QDoubleSpinBox* axisZ = nullptr;
  QDoubleSpinBox* radius = nullptr;
  QDoubleSpinBox* fillValue = nullptr;

  int extent[6] = { 0, 0, 0, 0, 0, 0 };
  double origin[3] = { 0, 0, 0 };
  double spacing[3] = { 1, 1, 1 };
  double dataBounds[6] = { 0, 0, 0, 0, 0, 0 };

  bool blockSignals = false;
};

SelectCylinderWidget::SelectCylinderWidget(
  const QMap<QString, pipeline::PortData>& inputs, QWidget* parent)
  : CustomPythonNodeWidget(parent), m_internal(new Internal)
{
  // Registered with needsData=true, so the host gates creation on
  // input availability — image is expected to be valid here.
  vtkSmartPointer<vtkImageData> image;
  if (auto it = inputs.constFind(QStringLiteral("volume"));
      it != inputs.constEnd()) {
    if (auto vol = it.value().value<pipeline::VolumeDataPtr>();
        vol && vol->isValid()) {
      image = vol->imageData();
    }
  }
  image->GetExtent(m_internal->extent);
  image->GetOrigin(m_internal->origin);
  image->GetSpacing(m_internal->spacing);

  int* ext = m_internal->extent;
  double* orig = m_internal->origin;
  double* sp = m_internal->spacing;

  int nx = ext[1] - ext[0] + 1;
  int ny = ext[3] - ext[2] + 1;
  int nz = ext[5] - ext[4] + 1;

  double defaultCenterX = (nx - 1) / 2.0;
  double defaultCenterY = (ny - 1) / 2.0;
  double defaultCenterZ = (nz - 1) / 2.0;
  double defaultRadius = qMin(nx, ny) / 2.0;

  double* bounds = m_internal->dataBounds;
  for (int i = 0; i < 3; ++i) {
    bounds[2 * i] = orig[i] + sp[i] * ext[2 * i];
    bounds[2 * i + 1] = orig[i] + sp[i] * ext[2 * i + 1];
  }

  vtkNew<vtkImplicitCylinderRepresentation> rep;
  rep->SetPlaceFactor(1.0);
  rep->PlaceWidget(bounds);
  rep->SetAxis(0, 0, 1);
  double center[3] = { orig[0] + sp[0] * defaultCenterX,
                        orig[1] + sp[1] * defaultCenterY,
                        orig[2] + sp[2] * defaultCenterZ };
  rep->SetCenter(center);
  rep->SetRadius(defaultRadius * sp[0]);
  rep->SetDrawCylinder(true);
  rep->SetScaleEnabled(false);
  rep->SetTubing(true);
  rep->SetResolution(64);
  rep->GetCylinderProperty()->SetOpacity(0.3);

  // Hide the rectangular outline and constrain to data bounds
  rep->GetOutlineProperty()->SetOpacity(0.0);
  rep->GetSelectedOutlineProperty()->SetOpacity(0.0);
  rep->SetOutlineTranslation(false);
  rep->SetOutsideBounds(false);
  rep->SetConstrainToWidgetBounds(true);

  vtkRenderWindowInteractor* iren =
    ActiveObjects::instance().activeView()->GetRenderWindow()->GetInteractor();
  m_internal->interactor = iren;

  m_internal->cylinderWidget->SetRepresentation(rep.GetPointer());
  m_internal->cylinderWidget->SetInteractor(iren);
  m_internal->cylinderWidget->SetPriority(1);
  m_internal->cylinderWidget->setupInteraction();
  m_internal->cylinderWidget->EnabledOn();

  m_internal->eventLink->Connect(
    m_internal->cylinderWidget.GetPointer(), vtkCommand::InteractionEvent, this,
    SLOT(onInteractionEnd()));

  iren->GetRenderWindow()->Render();

  // Build the Qt UI
  auto* layout = new QVBoxLayout;

  auto* instructions = new QLabel(
    "Left-click the cylinder and drag to translate. "
    "Left-click the arrow and drag to adjust the orientation. "
    "Right-click the cylinder and drag to adjust the radius.");
  instructions->setWordWrap(true);
  layout->addWidget(instructions);

  auto* grid = new QGridLayout;

  int row = 0;

  grid->addWidget(new QLabel("Center X:"), row, 0);
  m_internal->centerX = new QDoubleSpinBox;
  m_internal->centerX->setRange(-(double)nx, 2.0 * nx);
  m_internal->centerX->setDecimals(1);
  m_internal->centerX->setValue(defaultCenterX);
  grid->addWidget(m_internal->centerX, row++, 1);

  grid->addWidget(new QLabel("Center Y:"), row, 0);
  m_internal->centerY = new QDoubleSpinBox;
  m_internal->centerY->setRange(-(double)ny, 2.0 * ny);
  m_internal->centerY->setDecimals(1);
  m_internal->centerY->setValue(defaultCenterY);
  grid->addWidget(m_internal->centerY, row++, 1);

  grid->addWidget(new QLabel("Center Z:"), row, 0);
  m_internal->centerZ = new QDoubleSpinBox;
  m_internal->centerZ->setRange(-(double)nz, 2.0 * nz);
  m_internal->centerZ->setDecimals(1);
  m_internal->centerZ->setValue(defaultCenterZ);
  grid->addWidget(m_internal->centerZ, row++, 1);

  grid->addWidget(new QLabel("Axis X:"), row, 0);
  m_internal->axisX = new QDoubleSpinBox;
  m_internal->axisX->setRange(-1.0, 1.0);
  m_internal->axisX->setDecimals(4);
  m_internal->axisX->setSingleStep(0.1);
  m_internal->axisX->setValue(0.0);
  grid->addWidget(m_internal->axisX, row++, 1);

  grid->addWidget(new QLabel("Axis Y:"), row, 0);
  m_internal->axisY = new QDoubleSpinBox;
  m_internal->axisY->setRange(-1.0, 1.0);
  m_internal->axisY->setDecimals(4);
  m_internal->axisY->setSingleStep(0.1);
  m_internal->axisY->setValue(0.0);
  grid->addWidget(m_internal->axisY, row++, 1);

  grid->addWidget(new QLabel("Axis Z:"), row, 0);
  m_internal->axisZ = new QDoubleSpinBox;
  m_internal->axisZ->setRange(-1.0, 1.0);
  m_internal->axisZ->setDecimals(4);
  m_internal->axisZ->setSingleStep(0.1);
  m_internal->axisZ->setValue(1.0);
  grid->addWidget(m_internal->axisZ, row++, 1);

  grid->addWidget(new QLabel("Radius:"), row, 0);
  m_internal->radius = new QDoubleSpinBox;
  m_internal->radius->setRange(0.1, qMax(nx, qMax(ny, nz)));
  m_internal->radius->setDecimals(1);
  m_internal->radius->setValue(defaultRadius);
  grid->addWidget(m_internal->radius, row++, 1);

  grid->addWidget(new QLabel("Fill Value:"), row, 0);
  m_internal->fillValue = new QDoubleSpinBox;
  m_internal->fillValue->setRange(-1e12, 1e12);
  m_internal->fillValue->setDecimals(4);
  m_internal->fillValue->setValue(0.0);
  grid->addWidget(m_internal->fillValue, row++, 1);

  layout->addLayout(grid);
  layout->addStretch();
  setLayout(layout);

  connect(m_internal->centerX, &QDoubleSpinBox::editingFinished, this,
          &SelectCylinderWidget::onSpinBoxChanged);
  connect(m_internal->centerY, &QDoubleSpinBox::editingFinished, this,
          &SelectCylinderWidget::onSpinBoxChanged);
  connect(m_internal->centerZ, &QDoubleSpinBox::editingFinished, this,
          &SelectCylinderWidget::onSpinBoxChanged);
  connect(m_internal->axisX, &QDoubleSpinBox::editingFinished, this,
          &SelectCylinderWidget::onSpinBoxChanged);
  connect(m_internal->axisY, &QDoubleSpinBox::editingFinished, this,
          &SelectCylinderWidget::onSpinBoxChanged);
  connect(m_internal->axisZ, &QDoubleSpinBox::editingFinished, this,
          &SelectCylinderWidget::onSpinBoxChanged);
  connect(m_internal->radius, &QDoubleSpinBox::editingFinished, this,
          &SelectCylinderWidget::onSpinBoxChanged);
}

SelectCylinderWidget::~SelectCylinderWidget()
{
  disableWidget();
}

void SelectCylinderWidget::writeSettings()
{
  disableWidget();
}

void SelectCylinderWidget::disableWidget()
{
  m_internal->eventLink->Disconnect();
  m_internal->cylinderWidget->EnabledOff();
  m_internal->cylinderWidget->SetInteractor(nullptr);
  if (m_internal->interactor) {
    m_internal->interactor->GetRenderWindow()->Render();
  }
}

void SelectCylinderWidget::getValues(QMap<QString, QVariant>& map)
{
  map["center_x"] = m_internal->centerX->value();
  map["center_y"] = m_internal->centerY->value();
  map["center_z"] = m_internal->centerZ->value();
  map["axis_x"] = m_internal->axisX->value();
  map["axis_y"] = m_internal->axisY->value();
  map["axis_z"] = m_internal->axisZ->value();
  map["radius"] = m_internal->radius->value();
  map["fill_value"] = m_internal->fillValue->value();
}

void SelectCylinderWidget::setValues(const QMap<QString, QVariant>& map)
{
  if (map.contains("center_x")) {
    double cx = map["center_x"].toDouble();
    if (cx >= 0) {
      m_internal->centerX->setValue(cx);
    }
  }
  if (map.contains("center_y")) {
    double cy = map["center_y"].toDouble();
    if (cy >= 0) {
      m_internal->centerY->setValue(cy);
    }
  }
  if (map.contains("center_z")) {
    double cz = map["center_z"].toDouble();
    if (cz >= 0) {
      m_internal->centerZ->setValue(cz);
    }
  }
  if (map.contains("axis_x")) {
    m_internal->axisX->setValue(map["axis_x"].toDouble());
  }
  if (map.contains("axis_y")) {
    m_internal->axisY->setValue(map["axis_y"].toDouble());
  }
  if (map.contains("axis_z")) {
    m_internal->axisZ->setValue(map["axis_z"].toDouble());
  }
  if (map.contains("radius")) {
    double r = map["radius"].toDouble();
    if (r > 0) {
      m_internal->radius->setValue(r);
    }
  }
  if (map.contains("fill_value")) {
    m_internal->fillValue->setValue(map["fill_value"].toDouble());
  }

  onSpinBoxChanged();
}

void SelectCylinderWidget::onInteractionEnd()
{
  if (m_internal->blockSignals) {
    return;
  }

  auto* rep = m_internal->cylinderWidget->GetCylinderRepresentation();

  // Re-clamp the bounding box to data bounds so the cylinder is always
  // visually clipped against the dataset extent.
  double center[3], axis[3];
  rep->GetCenter(center);
  rep->GetAxis(axis);
  double radius = rep->GetRadius();

  rep->PlaceWidget(m_internal->dataBounds);
  rep->SetCenter(center);
  rep->SetAxis(axis);
  rep->SetRadius(radius);
  rep->BuildRepresentation();

  double* orig = m_internal->origin;
  double* sp = m_internal->spacing;

  // Convert from physical space to voxel coordinates
  double cx = (center[0] - orig[0]) / sp[0];
  double cy = (center[1] - orig[1]) / sp[1];
  double cz = (center[2] - orig[2]) / sp[2];

  // Convert axis from physical space to voxel space direction
  double axVoxel[3] = { axis[0] * sp[0], axis[1] * sp[1], axis[2] * sp[2] };
  double axLen = vtkMath::Norm(axVoxel);
  if (axLen > 1e-12) {
    axVoxel[0] /= axLen;
    axVoxel[1] /= axLen;
    axVoxel[2] /= axLen;
  }

  double avgSpacing = (sp[0] + sp[1]) / 2.0;
  double r = radius / avgSpacing;

  m_internal->blockSignals = true;
  m_internal->centerX->setValue(cx);
  m_internal->centerY->setValue(cy);
  m_internal->centerZ->setValue(cz);
  m_internal->axisX->setValue(axVoxel[0]);
  m_internal->axisY->setValue(axVoxel[1]);
  m_internal->axisZ->setValue(axVoxel[2]);
  m_internal->radius->setValue(r);
  m_internal->blockSignals = false;
}

void SelectCylinderWidget::onSpinBoxChanged()
{
  if (m_internal->blockSignals) {
    return;
  }

  double* orig = m_internal->origin;
  double* sp = m_internal->spacing;

  double cx = m_internal->centerX->value();
  double cy = m_internal->centerY->value();
  double cz = m_internal->centerZ->value();
  double ax = m_internal->axisX->value();
  double ay = m_internal->axisY->value();
  double az = m_internal->axisZ->value();
  double r = m_internal->radius->value();

  auto* rep = m_internal->cylinderWidget->GetCylinderRepresentation();

  double center[3] = { orig[0] + sp[0] * cx,
                        orig[1] + sp[1] * cy,
                        orig[2] + sp[2] * cz };

  // Convert axis direction from voxel to physical space
  double axPhys[3] = { ax / sp[0], ay / sp[1], az / sp[2] };
  double axLen = vtkMath::Norm(axPhys);
  if (axLen > 1e-12) {
    axPhys[0] /= axLen;
    axPhys[1] /= axLen;
    axPhys[2] /= axLen;
  }

  double avgSpacing = (sp[0] + sp[1]) / 2.0;

  m_internal->blockSignals = true;
  rep->PlaceWidget(m_internal->dataBounds);
  rep->SetCenter(center);
  rep->SetAxis(axPhys);
  rep->SetRadius(r * avgSpacing);
  rep->BuildRepresentation();
  m_internal->interactor->GetRenderWindow()->Render();
  m_internal->blockSignals = false;
}

} // namespace tomviz
