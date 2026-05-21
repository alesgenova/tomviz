/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ScaleCubeSink.h"

#include "data/VolumeData.h"

#include <vtkBillboardTextActor3D.h>
#include <vtkCommand.h>
#include <vtkHandleWidget.h>
#include <vtkMeasurementCubeHandleRepresentation3D.h>
#include <vtkPVRenderView.h>
#include <vtkProperty.h>
#include <vtkTextProperty.h>

#include <pqColorChooserButton.h>

#include <QCheckBox>
#include <QColor>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace tomviz {
namespace pipeline {

ScaleCubeSink::ScaleCubeSink(QObject* parent) : LegacyModuleSink(parent)
{
  addInput("volume", PortType::ImageData);
  setLabel("Scale Cube");

  m_cubeRep->SetAdaptiveScaling(0);

  // Observe modifications on the representation (user drags the cube)
  // to emit position/sideLength change signals.
  m_observedId = m_cubeRep->AddObserver(
    vtkCommand::ModifiedEvent, this, &ScaleCubeSink::observeModified);
}

ScaleCubeSink::~ScaleCubeSink()
{
  m_cubeRep->RemoveObserver(m_observedId);
  finalize();
}

QIcon ScaleCubeSink::icon() const
{
  return QIcon(QStringLiteral(":/icons/pqMeasurementCube.png"));
}

void ScaleCubeSink::setVisibility(bool visible)
{
  m_cubeRep->SetHandleVisibility(visible ? 1 : 0);
  if (!visible || m_annotationVisibility) {
    m_cubeRep->SetLabelVisibility(visible ? 1 : 0);
  }
  LegacyModuleSink::setVisibility(visible);
}

bool ScaleCubeSink::initialize(vtkSMViewProxy* view)
{
  if (!LegacyModuleSink::initialize(view)) {
    return false;
  }

  m_handleWidget->SetInteractor(renderView()->GetInteractor());
  m_handleWidget->SetRepresentation(m_cubeRep);
  m_handleWidget->EnabledOn();
  return true;
}

bool ScaleCubeSink::finalize()
{
  m_handleWidget->EnabledOff();
  return LegacyModuleSink::finalize();
}

void ScaleCubeSink::clearVisualization()
{
  m_cubeRep->SetHandleVisibility(0);
  m_cubeRep->SetLabelVisibility(0);
}

bool ScaleCubeSink::consume(const QMap<QString, PortData>& inputs)
{
  if (!validateInput(inputs, "volume")) {
    return false;
  }

  auto volume = inputs["volume"].value<VolumeDataPtr>();
  if (!volume || !volume->isValid()) {
    return false;
  }

  auto bounds = volume->bounds();

  if (m_firstConsume) {
    // Match old ModuleScaleCube::initialize(): 10% of X extent, floored.
    double extentX = bounds[1] - bounds[0];
    double length = std::max(std::floor(extentX * 0.1), 1.0);
    m_cubeRep->SetSideLength(length);
    emit sideLengthChanged(length);

    // Position at lower corner + half side length offset
    double pos[3];
    pos[0] = bounds[0] + 0.5 * length;
    pos[1] = bounds[2] + 0.5 * length;
    pos[2] = bounds[4] + 0.5 * length;
    m_cubeRep->PlaceWidget(pos);
    m_cubeRep->SetWorldPosition(pos);
    emit positionChanged(pos[0], pos[1], pos[2]);

    m_firstConsume = false;
  }

  // Update length unit from volume data
  auto unit = volume->units();
  if (!unit.isEmpty() && unit != m_lengthUnit) {
    m_lengthUnit = unit;
    m_cubeRep->SetLengthUnit(m_lengthUnit.toStdString().c_str());
    emit lengthUnitChanged(m_lengthUnit);
  }

  m_cubeRep->SetHandleVisibility(visibility() ? 1 : 0);
  m_cubeRep->SetLabelVisibility(
    (visibility() && m_annotationVisibility) ? 1 : 0);

  emit renderNeeded();
  return true;
}

void ScaleCubeSink::observeModified()
{
  double p[3];
  m_cubeRep->GetWorldPosition(p);
  emit positionChanged(p[0], p[1], p[2]);
  emit sideLengthChanged(m_cubeRep->GetSideLength());
}

double ScaleCubeSink::sideLength() const
{
  return m_cubeRep->GetSideLength();
}

void ScaleCubeSink::setSideLength(double length)
{
  m_cubeRep->SetSideLength(length);
  emit sideLengthChanged(length);
  emit renderNeeded();
}

void ScaleCubeSink::position(double pos[3]) const
{
  m_cubeRep->GetWorldPosition(pos);
}

void ScaleCubeSink::setPosition(double x, double y, double z)
{
  double pos[3] = { x, y, z };
  m_cubeRep->SetWorldPosition(pos);
  emit positionChanged(x, y, z);
  emit renderNeeded();
}

bool ScaleCubeSink::adaptiveScaling() const
{
  return m_cubeRep->GetAdaptiveScaling() == 1;
}

void ScaleCubeSink::setAdaptiveScaling(bool adaptive)
{
  m_cubeRep->SetAdaptiveScaling(adaptive ? 1 : 0);
}

void ScaleCubeSink::color(double rgb[3]) const
{
  m_cubeRep->GetProperty()->GetDiffuseColor(rgb);
}

void ScaleCubeSink::setColor(double r, double g, double b)
{
  m_cubeRep->GetProperty()->SetDiffuseColor(r, g, b);
  emit renderNeeded();
}

void ScaleCubeSink::textColor(double rgb[3]) const
{
  m_cubeRep->GetLabelText()->GetTextProperty()->GetColor(rgb);
}

void ScaleCubeSink::setTextColor(double r, double g, double b)
{
  m_cubeRep->GetLabelText()->GetTextProperty()->SetColor(r, g, b);
  emit renderNeeded();
}

bool ScaleCubeSink::showAnnotation() const
{
  return m_annotationVisibility;
}

void ScaleCubeSink::setShowAnnotation(bool show)
{
  m_annotationVisibility = show;
  m_cubeRep->SetLabelVisibility(show ? 1 : 0);
  emit renderNeeded();
}

QString ScaleCubeSink::lengthUnit() const
{
  return m_lengthUnit;
}

void ScaleCubeSink::setLengthUnit(const QString& unit)
{
  m_lengthUnit = unit;
  m_cubeRep->SetLengthUnit(unit.toStdString().c_str());
  emit lengthUnitChanged(unit);
  emit renderNeeded();
}

void ScaleCubeSink::onMetadataChanged()
{
  auto vol = volumeData();
  if (!vol) {
    return;
  }
  auto unit = vol->units();
  if (unit != m_lengthUnit) {
    setLengthUnit(unit);
  }
}

QWidget* ScaleCubeSink::createSinkPropertiesWidget(QWidget* parent)
{
  auto* widget = new QWidget(parent);
  auto* layout = new QFormLayout(widget);

  // --- Adaptive Scaling ---
  auto* adaptiveScalingCheck = new QCheckBox(widget);
  adaptiveScalingCheck->setChecked(adaptiveScaling());
  layout->addRow("Adaptive scaling", adaptiveScalingCheck);

  // --- Side Length ---
  auto* lengthLabelWidget = new QWidget(widget);
  auto* lengthLabelLayout = new QHBoxLayout(lengthLabelWidget);
  lengthLabelLayout->setContentsMargins(0, 0, 0, 0);
  lengthLabelLayout->setSpacing(0);
  lengthLabelLayout->addWidget(new QLabel("Length of side (", lengthLabelWidget));
  auto* lengthUnitLabel = new QLabel(m_lengthUnit, lengthLabelWidget);
  lengthLabelLayout->addWidget(lengthUnitLabel);
  lengthLabelLayout->addWidget(new QLabel(")", lengthLabelWidget));

  auto* sideLengthEdit = new QLineEdit(widget);
  sideLengthEdit->setValidator(new QDoubleValidator(widget));
  sideLengthEdit->setText(QString::number(sideLength()));
  layout->addRow(lengthLabelWidget, sideLengthEdit);

  // --- Position ---
  auto* posLabelWidget = new QWidget(widget);
  auto* posLabelLayout = new QHBoxLayout(posLabelWidget);
  posLabelLayout->setContentsMargins(0, 0, 0, 0);
  posLabelLayout->setSpacing(0);
  posLabelLayout->addWidget(new QLabel("Position (", posLabelWidget));
  auto* posUnitLabel = new QLabel(m_lengthUnit, posLabelWidget);
  posLabelLayout->addWidget(posUnitLabel);
  posLabelLayout->addWidget(new QLabel(")", posLabelWidget));
  layout->addRow(posLabelWidget, new QWidget(widget)); // label-only row

  double pos[3];
  position(pos);
  QLineEdit* posInputs[3];
  auto* posRow = new QHBoxLayout;
  const char* posLabels[] = { "X:", "Y:", "Z:" };
  for (int i = 0; i < 3; ++i) {
    posRow->addWidget(new QLabel(posLabels[i], widget));
    auto* edit = new QLineEdit(QString::number(pos[i]), widget);
    edit->setValidator(new QDoubleValidator(edit));
    posRow->addWidget(edit);
    posInputs[i] = edit;
  }
  layout->addRow(posRow);

  // --- Annotation ---
  auto* annotationCheck = new QCheckBox(widget);
  annotationCheck->setChecked(m_annotationVisibility);
  layout->addRow("Annotation", annotationCheck);

  // --- Box Color ---
  auto* boxColorButton = new pqColorChooserButton(widget);
  boxColorButton->setText("...");
  double rgb[3];
  color(rgb);
  boxColorButton->setChosenColor(
    QColor(static_cast<int>(rgb[0] * 255.0 + 0.5),
           static_cast<int>(rgb[1] * 255.0 + 0.5),
           static_cast<int>(rgb[2] * 255.0 + 0.5)));
  layout->addRow("Box Color", boxColorButton);

  // --- Text Color ---
  auto* textColorButton = new pqColorChooserButton(widget);
  textColorButton->setText("...");
  double tc[3];
  textColor(tc);
  textColorButton->setChosenColor(
    QColor(static_cast<int>(tc[0] * 255.0 + 0.5),
           static_cast<int>(tc[1] * 255.0 + 0.5),
           static_cast<int>(tc[2] * 255.0 + 0.5)));
  layout->addRow("Text Color", textColorButton);

  // --- Connections: UI → Sink ---
  QObject::connect(
    adaptiveScalingCheck, &QCheckBox::toggled,
    [this](bool checked) { setAdaptiveScaling(checked); });

  QObject::connect(
    sideLengthEdit, &QLineEdit::editingFinished,
    [this, sideLengthEdit]() {
      setSideLength(sideLengthEdit->text().toDouble());
    });

  QObject::connect(
    annotationCheck, &QCheckBox::toggled,
    [this](bool checked) { setShowAnnotation(checked); });

  QObject::connect(
    boxColorButton, &pqColorChooserButton::chosenColorChanged,
    [this](const QColor& c) {
      setColor(c.red() / 255.0, c.green() / 255.0, c.blue() / 255.0);
    });

  QObject::connect(
    textColorButton, &pqColorChooserButton::chosenColorChanged,
    [this](const QColor& c) {
      setTextColor(c.red() / 255.0, c.green() / 255.0, c.blue() / 255.0);
    });

  // Position text fields → Sink
  auto updatePosFn = [this, posInputs]() {
    setPosition(posInputs[0]->text().toDouble(),
                posInputs[1]->text().toDouble(),
                posInputs[2]->text().toDouble());
  };
  for (int i = 0; i < 3; ++i) {
    QObject::connect(posInputs[i], &QLineEdit::editingFinished, updatePosFn);
  }

  // --- Connections: Sink → UI ---
  QObject::connect(
    this, &ScaleCubeSink::sideLengthChanged,
    sideLengthEdit, [sideLengthEdit](double length) {
      QSignalBlocker b(sideLengthEdit);
      sideLengthEdit->setText(QString::number(length));
    });

  // Update position text fields when the cube is dragged in the 3D view
  QObject::connect(
    this, &ScaleCubeSink::positionChanged,
    widget, [posInputs](double x, double y, double z) {
      double vals[3] = { x, y, z };
      for (int i = 0; i < 3; ++i) {
        QSignalBlocker b(posInputs[i]);
        posInputs[i]->setText(QString::number(vals[i]));
      }
    });

  QObject::connect(
    this, &ScaleCubeSink::lengthUnitChanged,
    lengthUnitLabel, [lengthUnitLabel](const QString& unit) {
      lengthUnitLabel->setText(unit);
    });

  QObject::connect(
    this, &ScaleCubeSink::lengthUnitChanged,
    posUnitLabel, [posUnitLabel](const QString& unit) {
      posUnitLabel->setText(unit);
    });

  return widget;
}

QJsonObject ScaleCubeSink::serialize() const
{
  auto json = LegacyModuleSink::serialize();
  json["adaptiveScaling"] = adaptiveScaling();
  json["sideLength"] = sideLength();
  json["annotation"] = m_annotationVisibility;
  json["lengthUnit"] = m_lengthUnit;
  double pos[3];
  m_cubeRep->GetWorldPosition(pos);
  json["position"] = QJsonArray{ pos[0], pos[1], pos[2] };
  double rgb[3];
  m_cubeRep->GetProperty()->GetDiffuseColor(rgb);
  json["color"] = QJsonArray{ rgb[0], rgb[1], rgb[2] };
  double tc[3];
  m_cubeRep->GetLabelText()->GetTextProperty()->GetColor(tc);
  json["textColor"] = QJsonArray{ tc[0], tc[1], tc[2] };
  return json;
}

bool ScaleCubeSink::deserialize(const QJsonObject& json)
{
  if (!LegacyModuleSink::deserialize(json)) {
    return false;
  }
  if (json.contains("adaptiveScaling")) {
    setAdaptiveScaling(json["adaptiveScaling"].toBool());
  }
  if (json.contains("sideLength")) {
    setSideLength(json["sideLength"].toDouble());
  }
  if (json.contains("annotation")) {
    setShowAnnotation(json["annotation"].toBool());
  }
  if (json.contains("lengthUnit")) {
    setLengthUnit(json["lengthUnit"].toString());
  }
  if (json.value("position").isArray()) {
    auto arr = json.value("position").toArray();
    if (arr.size() == 3) {
      setPosition(arr.at(0).toDouble(), arr.at(1).toDouble(),
                  arr.at(2).toDouble());
    }
  }
  if (json.value("color").isArray()) {
    auto arr = json.value("color").toArray();
    if (arr.size() == 3) {
      setColor(arr.at(0).toDouble(), arr.at(1).toDouble(),
               arr.at(2).toDouble());
    }
  }
  if (json.value("textColor").isArray()) {
    auto arr = json.value("textColor").toArray();
    if (arr.size() == 3) {
      setTextColor(arr.at(0).toDouble(), arr.at(1).toDouble(),
                   arr.at(2).toDouble());
    }
  }
  return true;
}

} // namespace pipeline
} // namespace tomviz
