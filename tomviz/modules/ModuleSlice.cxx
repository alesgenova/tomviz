/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleSlice.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "IntSliderWidget.h"
#include "ScalarsComboBox.h"
#include "Utilities.h"

#include <vtkAlgorithm.h>
#include <vtkCommand.h>
#include <vtkDataObject.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkNonOrthoImagePlaneWidget.h>
#include <vtkPassThrough.h>
#include <vtkPointData.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkScalarsToColors.h>
#include <vtkTrivialProducer.h>

#include <pqCoreUtilities.h>
#include <pqDoubleVectorPropertyWidget.h>
#include <pqLineEdit.h>
#include <pqProxiesWidget.h>
#include <vtkPVArrayInformation.h>
#include <vtkPVDataInformation.h>
#include <vtkPVDataSetAttributesInformation.h>
#include <vtkPVDiscretizableColorTransferFunction.h>
#include <vtkSMPVRepresentationProxy.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMTransferFunctionProxy.h>
#include <vtkSMViewProxy.h>

#include <QCheckBox>
#include <QDebug>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QVBoxLayout>

namespace tomviz {

ModuleSlice::ModuleSlice(QObject* parentObject) : Module(parentObject) {}

ModuleSlice::~ModuleSlice()
{
  finalize();
}

QIcon ModuleSlice::icon() const
{
  return QIcon(":/icons/pqSlice.png");
}

bool ModuleSlice::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  auto pxm = ActiveObjects::instance().proxyManager();

  m_imageData->ShallowCopy(
    vtkImageData::SafeDownCast(data->producer()->GetOutputDataObject(0)));

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  // Create the pass through filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "PassThrough"));

  // Create the Properties panel proxy
  m_propsPanelProxy.TakeReference(
    pxm->NewProxy("tomviz_proxies", "NonOrthogonalSlice"));

  pqCoreUtilities::connect(m_propsPanelProxy, vtkCommand::PropertyModifiedEvent,
                           this, SLOT(onPropertyChanged()));

  m_passThrough = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(m_passThrough);
  controller->PreInitializeProxy(m_passThrough);
  vtkSmartPointer<vtkPassThrough> filter;
  filter = vtkPassThrough::SafeDownCast(m_passThrough->GetClientSideObject());
  Q_ASSERT(filter);
  filter->SetInputData(m_imageData);
  controller->PostInitializeProxy(m_passThrough);
  controller->RegisterPipelineProxy(m_passThrough);

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(proxy)) {
    p->rename(label());
  }

  const bool widgetSetup = setupWidget(vtkView);

  if (widgetSetup) {
    m_widget->SetDisplayOffset(data->displayPosition());
    m_widget->On();
    m_widget->InteractionOn();
    onDirectionChanged(m_direction);
    pqCoreUtilities::connect(m_widget, vtkCommand::InteractionEvent, this,
                             SLOT(onPlaneChanged()));
    connect(data, SIGNAL(dataChanged()), this, SLOT(dataUpdated()));
    connect(data, SIGNAL(activeScalarsChanged()), SLOT(onScalarArrayChanged()));
  }

  Q_ASSERT(m_widget);
  return widgetSetup;
}

//  Should only be called from initialize after the PassThrough has been setup
bool ModuleSlice::setupWidget(vtkSMViewProxy* vtkView)
{
  vtkAlgorithm* passThroughAlg =
    vtkAlgorithm::SafeDownCast(m_passThrough->GetClientSideObject());

  vtkRenderWindowInteractor* rwi = vtkView->GetRenderWindow()->GetInteractor();

  if (!rwi || !passThroughAlg) {
    return false;
  }

  m_widget = vtkSmartPointer<vtkNonOrthoImagePlaneWidget>::New();

  // Set the interactor on the widget to be what the current
  // render window is using.
  m_widget->SetInteractor(rwi);

  // Setup the color of the border of the widget.
  {
    double color[3] = { 1, 0, 0 };
    m_widget->GetPlaneProperty()->SetColor(color);
  }

  // Turn texture interpolation to be linear.
  m_widget->TextureInterpolateOn();
  m_widget->SetResliceInterpolateToLinear();

  // Construct the transfer function proxy for the widget.
  vtkSMProxy* lut = colorMap();

  // Set the widgets lookup table to be the one that the transfer function
  // manager is using.
  vtkScalarsToColors* stc =
    vtkScalarsToColors::SafeDownCast(lut->GetClientSideObject());
  m_widget->SetLookupTable(stc);

  // Lastly we set up the input connection.
  m_widget->SetInputConnection(passThroughAlg->GetOutputPort());

  Q_ASSERT(rwi);
  Q_ASSERT(passThroughAlg);
  onPlaneChanged();
  return true;
}

void ModuleSlice::updateColorMap()
{
  Q_ASSERT(m_widget);

  // Construct the transfer function proxy for the widget
  vtkSMProxy* lut = colorMap();

  // set the widgets lookup table to be the one that the transfer function
  // manager is using
  vtkScalarsToColors* stc =
    vtkScalarsToColors::SafeDownCast(lut->GetClientSideObject());
  m_widget->SetLookupTable(stc);
}

bool ModuleSlice::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_passThrough);

  m_passThrough = nullptr;

  if (m_widget != nullptr) {
    m_widget->InteractionOff();
    m_widget->Off();
  }

  return true;
}

bool ModuleSlice::setVisibility(bool val)
{
  Q_ASSERT(m_widget);
  m_widget->SetEnabled(val ? 1 : 0);
  // update the state of the arrow as well since it cannot update when the
  // widget is not enabled
  if (val) {
    vtkSMPropertyHelper showProperty(m_propsPanelProxy, "ShowArrow");
    // Not this: it hides the plane as well as the arrow...
    // Widget->SetEnabled(showProperty.GetAsInt());
    m_widget->SetArrowVisibility(showProperty.GetAsInt());
    m_widget->SetInteraction(showProperty.GetAsInt());
  }
  return true;
}

bool ModuleSlice::visibility() const
{
  if (m_widget) {
    return m_widget->GetEnabled() != 0;
  } else {
    return false;
  }
}

void ModuleSlice::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }

  QVBoxLayout* layout = new QVBoxLayout;
  QFormLayout* formLayout = new QFormLayout;

  QWidget* container = new QWidget;
  container->setLayout(formLayout);
  layout->addWidget(container);
  formLayout->setContentsMargins(0, 0, 0, 0);

  m_opacityCheckBox = new QCheckBox("Map Opacity");
  formLayout->addRow(m_opacityCheckBox);

  QCheckBox* mapScalarsCheckBox = new QCheckBox("Color Map Data");
  formLayout->addRow(mapScalarsCheckBox);

  auto line = new QFrame;
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  formLayout->addRow(line);

  m_scalarsCombo = new ScalarsComboBox();
  m_scalarsCombo->setOptions(dataSource(), this);
  formLayout->addRow("Scalars", m_scalarsCombo);

  m_directionCombo = new QComboBox();
  std::vector<std::tuple<QString, Direction>> options = {
    std::make_tuple("XY Plane", Direction::XY),
    std::make_tuple("YZ Plane", Direction::YZ),
    std::make_tuple("XZ Plane", Direction::XZ),
    std::make_tuple("Custom", Direction::Custom)
  };
  for (size_t i = 0; i < options.size(); ++i) {
    auto label = std::get<0>(options[i]);
    auto data = std::get<1>(options[i]);
    QVariant qData;
    qData.setValue(std::get<1>(options[i]));
    m_directionCombo->addItem(label, qData);
    if (data == m_direction) {
      m_directionCombo->setCurrentIndex(i);
    }
  }
  formLayout->addRow("Direction", m_directionCombo);

  m_sliceSlider = new IntSliderWidget(true);
  m_sliceSlider->setLineEditWidth(50);
  m_sliceSlider->setPageStep(1);
  m_sliceSlider->setMinimum(0);
  m_sliceSlider->setValue(m_slice);
  int axis = directionAxis(m_direction);
  bool isOrtho = axis >= 0;
  if (isOrtho) {
    int dims[3];
    m_imageData->GetDimensions(dims);
    m_sliceSlider->setMaximum(dims[axis] - 1);
  }
  formLayout->addRow("Slice", m_sliceSlider);

  QCheckBox* showArrow = new QCheckBox("Show Arrow");
  formLayout->addRow(showArrow);

  m_Links.addPropertyLink(showArrow, "checked", SIGNAL(toggled(bool)),
                          m_propsPanelProxy,
                          m_propsPanelProxy->GetProperty("ShowArrow"), 0);
  connect(showArrow, &QCheckBox::toggled, this, &ModuleSlice::dataUpdated);

  QLabel* label = new QLabel("Point on Plane");
  layout->addWidget(label);
  QHBoxLayout* row = new QHBoxLayout;
  const char* labels[] = { "X:", "Y:", "Z:" };
  for (int i = 0; i < 3; ++i) {
    label = new QLabel(labels[i]);
    row->addWidget(label);
    pqLineEdit* inputBox = new pqLineEdit;
    inputBox->setEnabled(!isOrtho);
    inputBox->setValidator(new QDoubleValidator(inputBox));
    m_Links.addPropertyLink(
      inputBox, "text2", SIGNAL(textChanged(const QString&)), m_propsPanelProxy,
      m_propsPanelProxy->GetProperty("PointOnPlane"), i);
    connect(inputBox, &pqLineEdit::textChangedAndEditingFinished, this,
            &ModuleSlice::dataUpdated);
    row->addWidget(inputBox);
    m_pointInputs[i] = inputBox;
  }
  layout->addItem(row);

  label = new QLabel("Plane Normal");
  layout->addWidget(label);
  row = new QHBoxLayout;
  for (int i = 0; i < 3; ++i) {
    label = new QLabel(labels[i]);
    row->addWidget(label);
    pqLineEdit* inputBox = new pqLineEdit;
    inputBox->setEnabled(!isOrtho);
    inputBox->setValidator(new QDoubleValidator(inputBox));
    m_Links.addPropertyLink(
      inputBox, "text2", SIGNAL(textChanged(const QString&)), m_propsPanelProxy,
      m_propsPanelProxy->GetProperty("PlaneNormal"), i);
    connect(inputBox, &pqLineEdit::textChangedAndEditingFinished, this,
            &ModuleSlice::dataUpdated);
    row->addWidget(inputBox);
    m_normalInputs[i] = inputBox;
  }
  layout->addItem(row);

  m_Links.addPropertyLink(mapScalarsCheckBox, "checked", SIGNAL(toggled(bool)),
                          m_propsPanelProxy,
                          m_propsPanelProxy->GetProperty("MapScalars"), 0);
  connect(mapScalarsCheckBox, SIGNAL(toggled(bool)), this, SLOT(dataUpdated()));

  layout->addStretch();

  panel->setLayout(layout);

  connect(m_opacityCheckBox, &QCheckBox::toggled, this, [this](bool val) {
    m_mapOpacity = val;
    // Ensure the colormap is detached before applying opacity
    if (val) {
      setUseDetachedColorMap(val);
    }
    auto func = vtkPVDiscretizableColorTransferFunction::SafeDownCast(
      colorMap()->GetClientSideObject());
    func->SetEnableOpacityMapping(val);
    emit opacityEnforced(val);
    updateColorMap();
    emit renderNeeded();
  });

  m_opacityCheckBox->setChecked(m_mapOpacity);

  connect(m_scalarsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int idx) {
            setActiveScalars(m_scalarsCombo->itemData(idx).toInt());
            onScalarArrayChanged();
          });
  connect(m_directionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int idx) {
            Direction dir = m_directionCombo->itemData(idx).value<Direction>();
            onDirectionChanged(dir);
          });

  connect(m_sliceSlider, &IntSliderWidget::valueEdited, this,
          QOverload<int>::of(&ModuleSlice::onSliceChanged));
  connect(m_sliceSlider, &IntSliderWidget::valueChanged, this,
          QOverload<int>::of(&ModuleSlice::onSliceChanged));
}

void ModuleSlice::dataUpdated()
{
  m_Links.accept();
  m_widget->SetMapScalars(
    vtkSMPropertyHelper(m_propsPanelProxy->GetProperty("MapScalars"), 1)
      .GetAsInt());
  m_widget->UpdatePlacement();
  emit renderNeeded();
}

QJsonObject ModuleSlice::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  vtkSMPropertyHelper showProperty(m_propsPanelProxy, "ShowArrow");
  props["showArrow"] = showProperty.GetAsInt() != 0;

  // Serialize the plane
  double point[3];
  m_widget->GetOrigin(point);
  QJsonArray origin = { point[0], point[1], point[2] };
  m_widget->GetPoint1(point);
  QJsonArray point1 = { point[0], point[1], point[2] };
  m_widget->GetPoint2(point);
  QJsonArray point2 = { point[0], point[1], point[2] };

  props["origin"] = origin;
  props["point1"] = point1;
  props["point2"] = point2;
  props["mapScalars"] = m_widget->GetMapScalars() != 0;
  props["mapOpacity"] = m_mapOpacity;

  props["slice"] = m_slice;
  QVariant qData;
  qData.setValue(m_direction);
  props["direction"] = qData.toString();

  json["properties"] = props;
  return json;
}

bool ModuleSlice::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    vtkSMPropertyHelper showProperty(m_propsPanelProxy, "ShowArrow");
    showProperty.Set(props["showArrow"].toBool() ? 1 : 0);
    auto o = props["origin"].toArray();
    auto p1 = props["point1"].toArray();
    auto p2 = props["point2"].toArray();
    double origin[3] = { o[0].toDouble(), o[1].toDouble(), o[2].toDouble() };
    double point1[3] = { p1[0].toDouble(), p1[1].toDouble(), p1[2].toDouble() };
    double point2[3] = { p2[0].toDouble(), p2[1].toDouble(), p2[2].toDouble() };
    m_widget->SetOrigin(origin);
    m_widget->SetPoint1(point1);
    m_widget->SetPoint2(point2);
    m_widget->SetMapScalars(props["mapScalars"].toBool() ? 1 : 0);
    if (props.contains("mapOpacity")) {
      m_mapOpacity = props["mapOpacity"].toBool();
      if (m_opacityCheckBox) {
        m_opacityCheckBox->setChecked(m_mapOpacity);
      }
    }
    m_widget->UpdatePlacement();
    m_scalarsCombo->setOptions(dataSource(), this);
    if (props.contains("direction")) {
      Direction direction = stringToDirection(props["direction"].toString());
      onDirectionChanged(direction);
    }
    if (props.contains("slice")) {
      m_slice = props["slice"].toInt();
      onSliceChanged(m_slice);
    }
    onPlaneChanged();
    return true;
  }
  return false;
}

void ModuleSlice::onPropertyChanged()
{
  // Avoid recursive clobbering of the plane position
  if (m_ignoreSignals) {
    return;
  }
  m_ignoreSignals = true;
  vtkSMPropertyHelper showProperty(m_propsPanelProxy, "ShowArrow");
  if (m_widget->GetEnabled()) {
    // Not this: it hides the plane as well as the arrow...
    // Widget->SetEnabled(showProperty.GetAsInt());
    m_widget->SetArrowVisibility(showProperty.GetAsInt());
    m_widget->SetInteraction(showProperty.GetAsInt());
  }
  vtkSMPropertyHelper pointProperty(m_propsPanelProxy, "PointOnPlane");
  std::vector<double> centerPoint = pointProperty.GetDoubleArray();
  m_widget->SetCenter(&centerPoint[0]);
  vtkSMPropertyHelper normalProperty(m_propsPanelProxy, "PlaneNormal");
  std::vector<double> normalVector = normalProperty.GetDoubleArray();
  m_widget->SetNormal(&normalVector[0]);
  m_widget->UpdatePlacement();
  m_ignoreSignals = false;
}

void ModuleSlice::onPlaneChanged()
{
  // Avoid recursive clobbering of the plane position
  if (m_ignoreSignals) {
    return;
  }
  m_ignoreSignals = true;
  vtkSMPropertyHelper pointProperty(m_propsPanelProxy, "PointOnPlane");
  double* centerPoint = m_widget->GetCenter();
  pointProperty.Set(centerPoint, 3);
  vtkSMPropertyHelper normalProperty(m_propsPanelProxy, "PlaneNormal");
  double* normalVector = m_widget->GetNormal();
  normalProperty.Set(normalVector, 3);
  vtkSMPropertyHelper mapScalarsProperty(m_propsPanelProxy, "MapScalars");
  int mapScalars = m_widget->GetMapScalars();
  mapScalarsProperty.Set(mapScalars);

  // Adjust the slice slider if the slice has changed from dragging the arrow
  onSliceChanged(centerPoint);

  m_ignoreSignals = false;
}

void ModuleSlice::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  m_widget->SetDisplayOffset(pos);
}

vtkSmartPointer<vtkDataObject> ModuleSlice::getDataToExport()
{
  return m_widget->GetResliceOutput();
}

bool ModuleSlice::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return (proxy == m_passThrough.Get()) || (proxy == m_propsPanelProxy.Get());
}

std::string ModuleSlice::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == m_passThrough.Get()) {
    return "PassThrough";
  } else if (proxy == m_propsPanelProxy.Get()) {
    return "NonOrthoSlice";
  } else {
    qWarning(
      "Unknown proxy passed to module non-ortho-slice in save animation");
    return "";
  }
}

vtkSMProxy* ModuleSlice::getProxyForString(const std::string& str)
{
  if (str == "PassThrough") {
    return m_passThrough.Get();
  } else if (str == "NonOrthoSlice") {
    return m_propsPanelProxy.Get();
  } else {
    return nullptr;
  }
}

bool ModuleSlice::areScalarsMapped() const
{
  vtkSMPropertyHelper mapScalars(m_propsPanelProxy->GetProperty("MapScalars"));
  return mapScalars.GetAsInt() != 0;
}

void ModuleSlice::onScalarArrayChanged()
{
  QString arrayName;
  if (activeScalars() == Module::DEFAULT_SCALARS) {
    arrayName = dataSource()->activeScalars();
  } else {
    arrayName = dataSource()->scalarsName(activeScalars());
  }
  m_imageData->GetPointData()->SetActiveScalars(arrayName.toLatin1().data());
  emit renderNeeded();
}

void ModuleSlice::onDirectionChanged(Direction direction)
{
  m_direction = direction;
  int axis = directionAxis(direction);

  bool isOrtho = axis >= 0;

  for (int i = 0; i < 3; ++i) {
    if (m_pointInputs[i]) {
      m_pointInputs[i]->setEnabled(!isOrtho);
    }
    if (m_normalInputs[i]) {
      m_normalInputs[i]->setEnabled(!isOrtho);
    }
  }
  if (m_sliceSlider) {
    m_sliceSlider->setVisible(isOrtho);
  }

  m_widget->SetOrtho(axis);

  if (m_directionCombo) {
    if (direction != m_directionCombo->currentData().value<Direction>()) {
      for (int i = 0; i < m_directionCombo->count(); ++i) {
        Direction data = m_directionCombo->itemData(i).value<Direction>();
        if (data == direction) {
          m_directionCombo->setCurrentIndex(i);
        }
      }
    }
  }

  if (!isOrtho) {
    return;
  }

  int dims[3];
  m_imageData->GetDimensions(dims);

  double normal[3] = { 0, 0, 0 };
  int slice = 0;
  int maxSlice = 0;

  normal[axis] = 1;
  slice = dims[axis] / 2;
  maxSlice = dims[axis] - 1;

  m_widget->SetNormal(normal);
  if (m_sliceSlider) {
    m_sliceSlider->setMinimum(0);
    m_sliceSlider->setMaximum(maxSlice);
  }
  onSliceChanged(slice);
  onPlaneChanged();
  dataUpdated();
}

void ModuleSlice::onSliceChanged(int slice)
{
  m_slice = slice;
  int axis = directionAxis(m_direction);
  if (axis < 0) {
    return;
  }

  int dims[3];
  m_imageData->GetDimensions(dims);

  double bounds[6];
  m_imageData->GetBounds(bounds);

  double point[3] = {
    bounds[0] + (bounds[1] - bounds[0]) * (dims[0] / 2) / (dims[0] - 1),
    bounds[2] + (bounds[3] - bounds[2]) * (dims[1] / 2) / (dims[1] - 1),
    bounds[4] + (bounds[5] - bounds[4]) * (dims[2] / 2) / (dims[2] - 1),
  };
  point[axis] =
    bounds[2 * axis] +
    (bounds[2 * axis + 1] - bounds[2 * axis]) * slice / (dims[axis] - 1);

  if (m_sliceSlider) {
    m_sliceSlider->setValue(slice);
  }
  m_widget->SetCenter(point);
  onPlaneChanged();
  dataUpdated();
}

void ModuleSlice::onSliceChanged(double* point)
{
  int axis = directionAxis(m_direction);
  if (axis < 0) {
    return;
  }

  int dims[3];
  m_imageData->GetDimensions(dims);
  double bounds[6];
  m_imageData->GetBounds(bounds);
  int slice;

  slice = (dims[axis] - 1) * (point[axis] - bounds[2 * axis]) /
          (bounds[2 * axis + 1] - bounds[2 * axis]);
  if (slice != m_slice) {
    onSliceChanged(slice);
  }
}

int ModuleSlice::directionAxis(Direction direction)
{
  switch (direction) {
    case Direction::XY: {
      return 2;
    }
    case Direction::YZ: {
      return 0;
    }
    case Direction::XZ: {
      return 1;
    }
    default: {
      return -1;
    }
  }
}

ModuleSlice::Direction ModuleSlice::stringToDirection(const QString& name)
{
  if (name == "XY") {
    return Direction::XY;
  } else if (name == "YZ") {
    return Direction::YZ;
  } else if (name == "XZ") {
    return Direction::XZ;
  } else {
    return Direction::Custom;
  }
}

} // namespace tomviz
