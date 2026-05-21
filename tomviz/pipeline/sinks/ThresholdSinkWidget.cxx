/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ThresholdSinkWidget.h"
#include "ui_ThresholdSinkWidget.h"

#include "DoubleSliderWidget.h"

namespace tomviz {

ThresholdSinkWidget::ThresholdSinkWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::ThresholdSinkWidget)
{
  m_ui->setupUi(this);

  qobject_cast<QBoxLayout*>(QWidget::layout())->addStretch();

  const int leWidth = 50;
  m_ui->sliMinimum->setLineEditWidth(leWidth);
  m_ui->sliMaximum->setLineEditWidth(leWidth);
  m_ui->sliOpacity->setLineEditWidth(leWidth);
  m_ui->sliSpecular->setLineEditWidth(leWidth);

  // Only update when the user releases the slider
  m_ui->sliMinimum->setSliderTracking(false);
  m_ui->sliMaximum->setSliderTracking(false);

  m_ui->sliMinimum->setKeyboardTracking(false);
  m_ui->sliMaximum->setKeyboardTracking(false);

  QStringList labelsRepre;
  labelsRepre << tr("Surface") << tr("Wireframe") << tr("Points");

  for (const auto& label : labelsRepre)
    m_ui->cbRepresentation->addItem(label, label);

  // Clamp: lower can't exceed upper
  connect(m_ui->sliMinimum, &DoubleSliderWidget::valueEdited, this,
          [this](double val) {
            if (val > m_ui->sliMaximum->value()) {
              m_ui->sliMinimum->setValue(m_ui->sliMaximum->value());
            }
            emit minimumChanged(m_ui->sliMinimum->value());
          });

  // Clamp: upper can't be below lower
  connect(m_ui->sliMaximum, &DoubleSliderWidget::valueEdited, this,
          [this](double val) {
            if (val < m_ui->sliMinimum->value()) {
              m_ui->sliMaximum->setValue(m_ui->sliMinimum->value());
            }
            emit maximumChanged(m_ui->sliMaximum->value());
          });

  connect(m_ui->cbColorMapData, &QCheckBox::toggled, this,
          &ThresholdSinkWidget::colorMapDataToggled);
  connect(m_ui->cbRepresentation,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ThresholdSinkWidget::onRepresentationIndexChanged);
  connect(m_ui->sliOpacity, &DoubleSliderWidget::valueEdited, this,
          &ThresholdSinkWidget::opacityChanged);
  connect(m_ui->sliSpecular, &DoubleSliderWidget::valueEdited, this,
          &ThresholdSinkWidget::specularChanged);
  connect(m_ui->cbColorByArray, &QCheckBox::toggled, this,
          &ThresholdSinkWidget::colorByArrayToggled);
  connect(m_ui->comboColorByArray,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ThresholdSinkWidget::onColorByArrayIndexChanged);
  connect(m_ui->comboThresholdByArray,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ThresholdSinkWidget::onThresholdByArrayIndexChanged);
}

ThresholdSinkWidget::~ThresholdSinkWidget() = default;

void ThresholdSinkWidget::setThresholdRange(double range[2])
{
  m_ui->sliMinimum->setMinimum(range[0]);
  m_ui->sliMinimum->setMaximum(range[1]);
  m_ui->sliMaximum->setMinimum(range[0]);
  m_ui->sliMaximum->setMaximum(range[1]);
}

void ThresholdSinkWidget::setColorMapData(bool state)
{
  m_ui->cbColorMapData->setChecked(state);
}

void ThresholdSinkWidget::setMinimum(double value)
{
  m_ui->sliMinimum->setValue(value);
}

void ThresholdSinkWidget::setMaximum(double value)
{
  m_ui->sliMaximum->setValue(value);
}

void ThresholdSinkWidget::setRepresentation(const QString& representation)
{
  for (int i = 0; i < m_ui->cbRepresentation->count(); ++i) {
    if (m_ui->cbRepresentation->itemData(i).toString() == representation) {
      m_ui->cbRepresentation->setCurrentIndex(i);
      return;
    }
  }
}

void ThresholdSinkWidget::setOpacity(double value)
{
  m_ui->sliOpacity->setValue(value);
}

void ThresholdSinkWidget::setSpecular(double value)
{
  m_ui->sliSpecular->setValue(value);
}

void ThresholdSinkWidget::setThresholdByArrayOptions(
  const QStringList& scalars, int activeScalar)
{
  const QSignalBlocker blocker(m_ui->comboThresholdByArray);
  m_ui->comboThresholdByArray->clear();

  m_ui->comboThresholdByArray->addItem("Default", -1);
  for (int i = 0; i < scalars.size(); ++i)
    m_ui->comboThresholdByArray->addItem(scalars[i], i);

  int currentIndex = (activeScalar == -1) ? 0 : activeScalar + 1;
  m_ui->comboThresholdByArray->setCurrentIndex(currentIndex);
}

void ThresholdSinkWidget::setColorByArrayOptions(const QStringList& options)
{
  m_ui->comboColorByArray->clear();

  for (const auto& opt : options)
    m_ui->comboColorByArray->addItem(opt, opt);
}

void ThresholdSinkWidget::setThresholdByArrayValue(int val)
{
  for (int i = 0; i < m_ui->comboThresholdByArray->count(); ++i) {
    if (m_ui->comboThresholdByArray->itemData(i).toInt() == val) {
      m_ui->comboThresholdByArray->setCurrentIndex(i);
      return;
    }
  }
}

void ThresholdSinkWidget::setColorByArray(bool state)
{
  m_ui->cbColorByArray->setChecked(state);
}

void ThresholdSinkWidget::setColorByArrayName(const QString& name)
{
  if (name.isEmpty() || m_ui->comboColorByArray->count() == 0)
    return;

  for (int i = 0; i < m_ui->comboColorByArray->count(); ++i) {
    if (m_ui->comboColorByArray->itemData(i).toString() == name) {
      m_ui->comboColorByArray->setCurrentIndex(i);
      return;
    }
  }
}

void ThresholdSinkWidget::onRepresentationIndexChanged(int i)
{
  emit representationChanged(m_ui->cbRepresentation->itemData(i).toString());
}

void ThresholdSinkWidget::onThresholdByArrayIndexChanged(int i)
{
  emit thresholdByArrayValueChanged(
    m_ui->comboThresholdByArray->itemData(i).toInt());
}

void ThresholdSinkWidget::onColorByArrayIndexChanged(int i)
{
  emit colorByArrayNameChanged(
    m_ui->comboColorByArray->itemData(i).toString());
}

} // namespace tomviz
