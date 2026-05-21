/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OutlineSink.h"

#include "data/VolumeData.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <pqColorChooserButton.h>

#include <vtkActor.h>
#include <vtkGridAxesActor3D.h>
#include <vtkGridAxesHelper.h>
#include <vtkImageData.h>
#include <vtkOutlineFilter.h>
#include <vtkPVRenderView.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkTextProperty.h>

namespace tomviz {
namespace pipeline {

OutlineSink::OutlineSink(QObject* parent) : LegacyModuleSink(parent)
{
  addInput("volume", PortType::ImageData);
  setLabel("Outline");

  m_mapper->SetInputConnection(m_outlineFilter->GetOutputPort());
  m_actor->SetMapper(m_mapper);
  m_actor->SetVisibility(0);
  m_actor->SetProperty(m_property);
  // Off-white default color
  m_property->SetColor(0.9, 0.9, 0.9);

  // Configure grid axes actor
  m_gridAxes->SetVisibility(0);
  m_gridAxes->SetGenerateGrid(false);

  // Work around a bug in vtkGridAxesActor3D: GetProperty() returns the
  // vtkProperty associated with a single face. Use our own property
  // shared with the outline actor so all faces update together.
  m_property->SetFrontfaceCulling(1);
  m_property->SetBackfaceCulling(0);
  m_gridAxes->SetProperty(m_property);

  // Set mask to show labels on all axes
  m_gridAxes->SetLabelMask(vtkGridAxesHelper::LabelMasks::MIN_X |
                           vtkGridAxesHelper::LabelMasks::MIN_Y |
                           vtkGridAxesHelper::LabelMasks::MIN_Z |
                           vtkGridAxesHelper::LabelMasks::MAX_X |
                           vtkGridAxesHelper::LabelMasks::MAX_Y |
                           vtkGridAxesHelper::LabelMasks::MAX_Z);

  // Set mask to render all faces
  m_gridAxes->SetFaceMask(vtkGridAxesHelper::Faces::MAX_XY |
                          vtkGridAxesHelper::Faces::MAX_YZ |
                          vtkGridAxesHelper::Faces::MAX_ZX |
                          vtkGridAxesHelper::Faces::MIN_XY |
                          vtkGridAxesHelper::Faces::MIN_YZ |
                          vtkGridAxesHelper::Faces::MIN_ZX);

  // Set titles and initial color (off-white)
  updateGridAxesTitles();
  updateGridAxesColor(0.9, 0.9, 0.9);
}

OutlineSink::~OutlineSink()
{
  finalize();
}

QIcon OutlineSink::icon() const
{
  return QIcon(QStringLiteral(":/pqWidgets/Icons/pqProbeLocation.svg"));
}

void OutlineSink::setVisibility(bool visible)
{
  m_actor->SetVisibility(visible ? 1 : 0);
  m_gridAxes->SetVisibility((visible && m_showGridAxes) ? 1 : 0);
  LegacyModuleSink::setVisibility(visible);
}

bool OutlineSink::initialize(vtkSMViewProxy* view)
{
  if (!LegacyModuleSink::initialize(view)) {
    return false;
  }

  renderView()->AddPropToRenderer(m_actor);
  renderView()->AddPropToRenderer(m_gridAxes);
  return true;
}

bool OutlineSink::finalize()
{
  if (renderView()) {
    renderView()->RemovePropFromRenderer(m_actor);
    renderView()->RemovePropFromRenderer(m_gridAxes);
  }
  return LegacyModuleSink::finalize();
}

bool OutlineSink::consume(const QMap<QString, PortData>& inputs)
{
  if (!validateInput(inputs, "volume")) {
    return false;
  }

  auto volume = inputs["volume"].value<VolumeDataPtr>();
  if (!volume || !volume->isValid()) {
    return false;
  }

  m_outlineFilter->SetInputData(volume->imageData());

  // Update grid axes bounds to match the volume
  auto bounds = volume->bounds();
  m_gridAxes->SetGridBounds(bounds.data());
  updateGridAxesTitles();

  m_actor->SetVisibility(visibility() ? 1 : 0);
  m_gridAxes->SetVisibility(
    (visibility() && m_showGridAxes) ? 1 : 0);

  onMetadataChanged();
  emit renderNeeded();
  return true;
}

void OutlineSink::color(double rgb[3]) const
{
  m_property->GetColor(rgb);
}

void OutlineSink::setColor(double r, double g, double b)
{
  updateGridAxesColor(r, g, b);
  emit renderNeeded();
}

QWidget* OutlineSink::createSinkPropertiesWidget(QWidget* parent)
{
  auto* widget = new QWidget(parent);
  auto* panelLayout = new QVBoxLayout(widget);

  // --- Color ---
  auto* colorLayout = new QHBoxLayout;
  auto* colorLabel = new QLabel("Color", widget);
  colorLayout->addWidget(colorLabel);
  colorLayout->addStretch();

  auto* colorSelector = new pqColorChooserButton(widget);
  colorSelector->setShowAlphaChannel(false);
  double rgb[3];
  color(rgb);
  colorSelector->setChosenColor(
    QColor::fromRgbF(rgb[0], rgb[1], rgb[2]));
  QObject::connect(
    colorSelector, &pqColorChooserButton::chosenColorChanged,
    [this](const QColor& c) {
      setColor(c.redF(), c.greenF(), c.blueF());
    });
  colorLayout->addWidget(colorSelector);
  panelLayout->addLayout(colorLayout);

  // --- Show Axes ---
  auto* showAxesLayout = new QHBoxLayout;
  auto* showAxes = new QCheckBox("Show Axes", widget);
  showAxes->setChecked(showGridAxes());
  showAxesLayout->addWidget(showAxes);
  panelLayout->addLayout(showAxesLayout);

  // --- Show Grid ---
  auto* showGridLayout = new QHBoxLayout;
  auto* showGrid = new QCheckBox("Show Grid", widget);
  showGrid->setChecked(generateGrid());
  showGridLayout->addWidget(showGrid);
  panelLayout->addLayout(showGridLayout);

  // --- Custom Axes Titles ---
  auto* useCustomTitlesLayout = new QHBoxLayout;
  auto* useCustomTitles = new QCheckBox("Custom Axes Titles", widget);
  useCustomTitles->setChecked(useCustomAxesTitles());
  useCustomTitlesLayout->addWidget(useCustomTitles);
  panelLayout->addLayout(useCustomTitlesLayout);

  // --- Custom titles group box ---
  auto* titlesGroupBox = new QGroupBox(widget);
  auto* titlesLayout = new QVBoxLayout(titlesGroupBox);

  auto* xLayout = new QHBoxLayout;
  xLayout->addWidget(new QLabel("X:", widget));
  auto* xTitleEdit = new QLineEdit(xTitle(), widget);
  xLayout->addWidget(xTitleEdit);
  titlesLayout->addLayout(xLayout);

  auto* yLayout = new QHBoxLayout;
  yLayout->addWidget(new QLabel("Y:", widget));
  auto* yTitleEdit = new QLineEdit(yTitle(), widget);
  yLayout->addWidget(yTitleEdit);
  titlesLayout->addLayout(yLayout);

  auto* zLayout = new QHBoxLayout;
  zLayout->addWidget(new QLabel("Z:", widget));
  auto* zTitleEdit = new QLineEdit(zTitle(), widget);
  zLayout->addWidget(zTitleEdit);
  titlesLayout->addLayout(zLayout);

  titlesGroupBox->setVisible(useCustomAxesTitles());
  panelLayout->addWidget(titlesGroupBox);

  // Disable grid/custom titles when axes not shown
  if (!showAxes->isChecked()) {
    showGrid->setEnabled(false);
    useCustomTitles->setEnabled(false);
    titlesGroupBox->setVisible(false);
  }

  // --- Connections ---
  QObject::connect(
    showAxes, &QCheckBox::checkStateChanged,
    [this, showGrid, useCustomTitles](int state) {
      bool checked = state == Qt::Checked;
      setShowGridAxes(checked);
      if (!checked) {
        showGrid->setChecked(false);
        useCustomTitles->setChecked(false);
      }
      showGrid->setEnabled(checked);
      useCustomTitles->setEnabled(checked);
    });

  QObject::connect(showGrid, &QCheckBox::checkStateChanged,
                   [this](int state) {
                     setGenerateGrid(state == Qt::Checked);
                   });

  QObject::connect(useCustomTitles, &QCheckBox::toggled,
                   [this, titlesGroupBox](bool b) {
                     setUseCustomAxesTitles(b);
                     titlesGroupBox->setVisible(b);
                   });

  QObject::connect(xTitleEdit, &QLineEdit::textChanged,
                   [this](const QString& text) {
                     setXTitle(text);
                   });
  QObject::connect(yTitleEdit, &QLineEdit::textChanged,
                   [this](const QString& text) {
                     setYTitle(text);
                   });
  QObject::connect(zTitleEdit, &QLineEdit::textChanged,
                   [this](const QString& text) {
                     setZTitle(text);
                   });

  panelLayout->addStretch();
  return widget;
}

bool OutlineSink::showGridAxes() const
{
  return m_showGridAxes;
}

void OutlineSink::setShowGridAxes(bool show)
{
  m_showGridAxes = show;
  m_gridAxes->SetVisibility(
    (visibility() && m_showGridAxes) ? 1 : 0);
  emit renderNeeded();
}

QString OutlineSink::xTitle() const
{
  return m_xTitle;
}

void OutlineSink::setXTitle(const QString& title)
{
  m_xTitle = title;
  updateGridAxesTitles();
  emit renderNeeded();
}

QString OutlineSink::yTitle() const
{
  return m_yTitle;
}

void OutlineSink::setYTitle(const QString& title)
{
  m_yTitle = title;
  updateGridAxesTitles();
  emit renderNeeded();
}

QString OutlineSink::zTitle() const
{
  return m_zTitle;
}

void OutlineSink::setZTitle(const QString& title)
{
  m_zTitle = title;
  updateGridAxesTitles();
  emit renderNeeded();
}

bool OutlineSink::generateGrid() const
{
  return m_generateGrid;
}

void OutlineSink::setGenerateGrid(bool gen)
{
  m_generateGrid = gen;
  m_gridAxes->SetGenerateGrid(gen);
  emit renderNeeded();
}

bool OutlineSink::useCustomAxesTitles() const
{
  return m_useCustomAxesTitles;
}

void OutlineSink::setUseCustomAxesTitles(bool use)
{
  m_useCustomAxesTitles = use;
  updateGridAxesTitles();
  emit renderNeeded();
}

void OutlineSink::updateGridAxesColor(double r, double g, double b)
{
  m_property->SetColor(r, g, b);
  for (int i = 0; i < 6; ++i) {
    vtkNew<vtkTextProperty> prop;
    prop->SetColor(r, g, b);
    m_gridAxes->SetTitleTextProperty(i, prop);
    m_gridAxes->SetLabelTextProperty(i, prop);
  }
}

void OutlineSink::updateGridAxesTitles()
{
  QString xt, yt, zt;
  if (m_useCustomAxesTitles) {
    xt = m_xTitle;
    yt = m_yTitle;
    zt = m_zTitle;
  } else {
    auto vol = volumeData();
    QString units = vol ? vol->units() : QString();
    if (units.isEmpty()) {
      xt = "X";
      yt = "Y";
      zt = "Z";
    } else {
      xt = QString("X (%1)").arg(units);
      yt = QString("Y (%1)").arg(units);
      zt = QString("Z (%1)").arg(units);
    }
  }
  m_gridAxes->SetXTitle(xt.toUtf8().data());
  m_gridAxes->SetYTitle(yt.toUtf8().data());
  m_gridAxes->SetZTitle(zt.toUtf8().data());
}

QJsonObject OutlineSink::serialize() const
{
  auto json = LegacyModuleSink::serialize();
  double rgb[3];
  m_property->GetColor(rgb);
  json["gridColor"] = QJsonArray{ rgb[0], rgb[1], rgb[2] };
  json["gridVisibility"] = m_showGridAxes;
  json["gridLines"] = m_generateGrid;
  json["useCustomAxesTitles"] = m_useCustomAxesTitles;
  json["customXTitle"] = m_xTitle;
  json["customYTitle"] = m_yTitle;
  json["customZTitle"] = m_zTitle;
  return json;
}

bool OutlineSink::deserialize(const QJsonObject& json)
{
  if (!LegacyModuleSink::deserialize(json)) {
    return false;
  }
  if (json.value("gridColor").isArray()) {
    auto arr = json.value("gridColor").toArray();
    if (arr.size() == 3) {
      setColor(arr.at(0).toDouble(), arr.at(1).toDouble(),
               arr.at(2).toDouble());
    }
  }
  if (json.contains("gridVisibility")) {
    setShowGridAxes(json["gridVisibility"].toBool());
  }
  if (json.contains("gridLines")) {
    setGenerateGrid(json["gridLines"].toBool());
  }
  if (json.contains("useCustomAxesTitles")) {
    setUseCustomAxesTitles(json["useCustomAxesTitles"].toBool());
  }
  if (json.contains("customXTitle")) {
    setXTitle(json["customXTitle"].toString());
  }
  if (json.contains("customYTitle")) {
    setYTitle(json["customYTitle"].toString());
  }
  if (json.contains("customZTitle")) {
    setZTitle(json["customZTitle"].toString());
  }
  return true;
}

void OutlineSink::clearVisualization()
{
  m_actor->SetVisibility(0);
  m_gridAxes->SetVisibility(0);
}

void OutlineSink::onMetadataChanged()
{
  auto vol = volumeData();
  if (!vol) return;
  auto pos = vol->displayPosition();
  auto orient = vol->displayOrientation();
  m_actor->SetPosition(pos.data());
  m_actor->SetOrientation(orient.data());
  m_gridAxes->SetPosition(pos.data());
  m_gridAxes->SetOrientation(orient.data());
  emit renderNeeded();
}

} // namespace pipeline
} // namespace tomviz
