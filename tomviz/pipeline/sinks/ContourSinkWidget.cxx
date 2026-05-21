/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ContourSinkWidget.h"
#include "ui_ContourSinkWidget.h"
#include "ui_LightingParametersForm.h"

#include <QDebug>

namespace tomviz {

ContourSinkWidget::ContourSinkWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::ContourSinkWidget),
    m_uiLighting(new Ui::LightingParametersForm)
{
  m_ui->setupUi(this);

  QWidget* lightingWidget = new QWidget;
  m_uiLighting->setupUi(lightingWidget);
  m_uiLighting->gbLighting->setCheckable(false);
  QWidget::layout()->addWidget(lightingWidget);
  qobject_cast<QBoxLayout*>(QWidget::layout())->addStretch();

  m_ui->colorChooser->setShowAlphaChannel(false);

  const int leWidth = 50;
  m_ui->sliValue->setLineEditWidth(leWidth);
  m_ui->sliOpacity->setLineEditWidth(leWidth);
  m_uiLighting->sliAmbient->setLineEditWidth(leWidth);
  m_uiLighting->sliDiffuse->setLineEditWidth(leWidth);
  m_uiLighting->sliSpecular->setLineEditWidth(leWidth);
  m_uiLighting->sliSpecularPower->setLineEditWidth(leWidth);

  m_uiLighting->sliSpecularPower->setMaximum(150);
  m_uiLighting->sliSpecularPower->setMinimum(1);
  m_uiLighting->sliSpecularPower->setResolution(200);

  QStringList labelsRepre;
  labelsRepre << tr("Surface") << tr("Wireframe") << tr("Points");

  for (const auto& label : labelsRepre)
    m_ui->cbRepresentation->addItem(label, label);

  connect(m_ui->cbColorMapData, &QCheckBox::toggled, this,
          &ContourSinkWidget::colorMapDataToggled);
  connect(m_uiLighting->sliAmbient, &DoubleSliderWidget::valueEdited, this,
          &ContourSinkWidget::ambientChanged);
  connect(m_uiLighting->sliDiffuse, &DoubleSliderWidget::valueEdited, this,
          &ContourSinkWidget::diffuseChanged);
  connect(m_uiLighting->sliSpecular, &DoubleSliderWidget::valueEdited, this,
          &ContourSinkWidget::specularChanged);
  connect(m_uiLighting->sliSpecularPower, &DoubleSliderWidget::valueEdited,
          this, &ContourSinkWidget::specularPowerChanged);
  connect(m_ui->sliValue, &DoubleSliderWidget::valueEdited, this,
          &ContourSinkWidget::isoChanged);
  connect(m_ui->cbRepresentation,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ContourSinkWidget::onRepresentationIndexChanged);
  connect(m_ui->sliOpacity, &DoubleSliderWidget::valueEdited, this,
          &ContourSinkWidget::opacityChanged);
  connect(m_ui->colorChooser, &pqColorChooserButton::chosenColorChanged, this,
          &ContourSinkWidget::colorChanged);
  connect(m_ui->cbSelectColor, &QCheckBox::toggled, this,
          &ContourSinkWidget::useSolidColorToggled);
  connect(m_ui->cbColorByArray, &QCheckBox::toggled, this,
          &ContourSinkWidget::colorByArrayToggled);
  connect(m_ui->comboColorByArray,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ContourSinkWidget::onColorByArrayIndexChanged);
  connect(m_ui->comboContourByArray,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ContourSinkWidget::onContourByArrayIndexChanged);
}

ContourSinkWidget::~ContourSinkWidget() = default;

void ContourSinkWidget::setIsoRange(double range[2])
{
  m_ui->sliValue->setMinimum(range[0]);
  m_ui->sliValue->setMaximum(range[1]);
}

void ContourSinkWidget::setContourByArrayOptions(const QStringList& scalars,
                                                  int activeScalar)
{
  const QSignalBlocker blocker(m_ui->comboContourByArray);
  m_ui->comboContourByArray->clear();

  m_ui->comboContourByArray->addItem("Default", -1);
  for (int i = 0; i < scalars.size(); ++i)
    m_ui->comboContourByArray->addItem(scalars[i], i);

  int currentIndex = (activeScalar == -1) ? 0 : activeScalar + 1;
  m_ui->comboContourByArray->setCurrentIndex(currentIndex);
}

void ContourSinkWidget::setColorByArrayOptions(const QStringList& options)
{
  m_ui->comboColorByArray->clear();

  // Save and use the text in the item data
  for (const auto& opt : options)
    m_ui->comboColorByArray->addItem(opt, opt);
}

void ContourSinkWidget::setColorMapData(const bool state)
{
  m_ui->cbColorMapData->setChecked(state);
}

void ContourSinkWidget::setAmbient(const double value)
{
  m_uiLighting->sliAmbient->setValue(value);
}

void ContourSinkWidget::setDiffuse(const double value)
{
  m_uiLighting->sliDiffuse->setValue(value);
}

void ContourSinkWidget::setSpecular(const double value)
{
  m_uiLighting->sliSpecular->setValue(value);
}

void ContourSinkWidget::setSpecularPower(const double value)
{
  m_uiLighting->sliSpecularPower->setValue(value);
}

void ContourSinkWidget::setIso(const double value)
{
  m_ui->sliValue->setValue(value);
}

void ContourSinkWidget::setRepresentation(const QString& representation)
{
  for (int i = 0; i < m_ui->cbRepresentation->count(); ++i) {
    if (m_ui->cbRepresentation->itemData(i).toString() == representation) {
      m_ui->cbRepresentation->setCurrentIndex(i);
      return;
    }
  }

  qCritical() << "Could not find" << representation
              << "in representation options";
}

void ContourSinkWidget::setOpacity(const double value)
{
  m_ui->sliOpacity->setValue(value);
}

void ContourSinkWidget::setColor(const QColor& color)
{
  m_ui->colorChooser->setChosenColor(color);
}

void ContourSinkWidget::setUseSolidColor(const bool state)
{
  m_ui->cbSelectColor->setChecked(state);
}

void ContourSinkWidget::setColorByArray(const bool state)
{
  m_ui->cbColorByArray->setChecked(state);
}

void ContourSinkWidget::setColorByArrayName(const QString& name)
{
  if (name.isEmpty() || m_ui->comboColorByArray->count() == 0)
    return;

  for (int i = 0; i < m_ui->comboColorByArray->count(); ++i) {
    if (m_ui->comboColorByArray->itemData(i).toString() == name) {
      m_ui->comboColorByArray->setCurrentIndex(i);
      return;
    }
  }

  qCritical() << "Could not find" << name << "in ColorByArray options";
}

void ContourSinkWidget::setContourByArrayValue(int val)
{
  for (int i = 0; i < m_ui->comboContourByArray->count(); ++i) {
    if (m_ui->comboContourByArray->itemData(i).toInt() == val) {
      m_ui->comboContourByArray->setCurrentIndex(i);
      return;
    }
  }

  qCritical() << "Could not find" << val << "in ContourByArray options";
}

void ContourSinkWidget::onContourByArrayIndexChanged(int i)
{
  emit contourByArrayValueChanged(
    m_ui->comboContourByArray->itemData(i).toInt());
}

void ContourSinkWidget::onColorByArrayIndexChanged(int i)
{
  emit colorByArrayNameChanged(
    m_ui->comboColorByArray->itemData(i).toString());
}

void ContourSinkWidget::onRepresentationIndexChanged(int i)
{
  emit representationChanged(m_ui->cbRepresentation->itemData(i).toString());
}

} // namespace tomviz
