/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleVolumeWidget.h"
#include "Module.h"
#include "ui_LightingParametersForm.h"
#include "ui_ModuleVolumeWidget.h"

#include "vtkVolumeMapper.h"

namespace tomviz {

ModuleVolumeWidget::ModuleVolumeWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::ModuleVolumeWidget),
    m_uiLighting(new Ui::LightingParametersForm)
{
  m_ui->setupUi(this);

  QWidget* lightingWidget = new QWidget;
  m_uiLighting->setupUi(lightingWidget);
  QWidget::layout()->addWidget(lightingWidget);
  qobject_cast<QBoxLayout*>(QWidget::layout())->addStretch();

  const int leWidth = 50;
  m_uiLighting->sliAmbient->setLineEditWidth(leWidth);
  m_uiLighting->sliDiffuse->setLineEditWidth(leWidth);
  m_uiLighting->sliSpecular->setLineEditWidth(leWidth);
  m_uiLighting->sliSpecularPower->setLineEditWidth(leWidth);

  m_uiLighting->sliSpecularPower->setMaximum(150);
  m_uiLighting->sliSpecularPower->setMinimum(1);
  m_uiLighting->sliSpecularPower->setResolution(200);

  m_ui->soliditySlider->setLineEditWidth(leWidth);

  QStringList labelsBlending;
  labelsBlending << tr("Composite") << tr("Max") << tr("Min") << tr("Average")
                 << tr("Additive");
  m_ui->cbBlending->addItems(labelsBlending);

  QStringList labelsTransferMode;
  labelsTransferMode << tr("Scalar") << tr("Scalar-Gradient 1D")
                     << tr("Scalar-Gradient 2D");
  m_ui->cbTransferMode->addItems(labelsTransferMode);

  QStringList labelsInterp;
  labelsInterp << tr("Nearest Neighbor") << tr("Linear");
  m_ui->cbInterpolation->addItems(labelsInterp);

  connect(m_ui->cbJittering, SIGNAL(toggled(bool)), this,
          SIGNAL(jitteringToggled(const bool)));
  connect(m_ui->cbBlending, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onBlendingChanged(const int)));
  connect(m_ui->cbInterpolation, SIGNAL(currentIndexChanged(int)), this,
          SIGNAL(interpolationChanged(const int)));
  connect(m_ui->cbTransferMode, SIGNAL(currentIndexChanged(int)), this,
          SIGNAL(transferModeChanged(const int)));

  connect(m_ui->useRgbaMapping, &QCheckBox::toggled, this,
          &ModuleVolumeWidget::useRgbaMappingToggled);

  // Using QueuedConnections here to circumvent DoubleSliderWidget->BlockUpdate
  connect(m_ui->sliRgbaMappingMin, &DoubleSliderWidget::valueEdited, this,
          &ModuleVolumeWidget::onRgbaMappingMinChanged, Qt::QueuedConnection);
  connect(m_ui->sliRgbaMappingMax, &DoubleSliderWidget::valueEdited, this,
          &ModuleVolumeWidget::onRgbaMappingMaxChanged, Qt::QueuedConnection);

  connect(m_uiLighting->gbLighting, SIGNAL(toggled(bool)), this,
          SIGNAL(lightingToggled(const bool)));
  connect(m_uiLighting->sliAmbient, SIGNAL(valueEdited(double)), this,
          SIGNAL(ambientChanged(const double)));
  connect(m_uiLighting->sliDiffuse, SIGNAL(valueEdited(double)), this,
          SIGNAL(diffuseChanged(const double)));
  connect(m_uiLighting->sliSpecular, SIGNAL(valueEdited(double)), this,
          SIGNAL(specularChanged(const double)));
  connect(m_uiLighting->sliSpecularPower, SIGNAL(valueEdited(double)), this,
          SIGNAL(specularPowerChanged(const double)));
  connect(m_ui->soliditySlider, SIGNAL(valueEdited(double)), this,
          SIGNAL(solidityChanged(const double)));

  m_ui->groupRgbaMappingRange->setVisible(false);
}

ModuleVolumeWidget::~ModuleVolumeWidget() = default;

void ModuleVolumeWidget::setJittering(const bool enable)
{
  m_ui->cbJittering->setChecked(enable);
}

void ModuleVolumeWidget::setBlendingMode(const int mode)
{
  m_uiLighting->gbLighting->setEnabled(usesLighting(mode));
  m_ui->cbBlending->setCurrentIndex(static_cast<int>(mode));
}

void ModuleVolumeWidget::setInterpolationType(const int type)
{
  m_ui->cbInterpolation->setCurrentIndex(type);
}

void ModuleVolumeWidget::setLighting(const bool enable)
{
  m_uiLighting->gbLighting->setChecked(enable);
}

void ModuleVolumeWidget::setAmbient(const double value)
{
  m_uiLighting->sliAmbient->setValue(value);
}

void ModuleVolumeWidget::setDiffuse(const double value)
{
  m_uiLighting->sliDiffuse->setValue(value);
}

void ModuleVolumeWidget::setSpecular(const double value)
{
  m_uiLighting->sliSpecular->setValue(value);
}

void ModuleVolumeWidget::setSpecularPower(const double value)
{
  m_uiLighting->sliSpecularPower->setValue(value);
}

void ModuleVolumeWidget::onBlendingChanged(const int mode)
{
  m_uiLighting->gbLighting->setEnabled(usesLighting(mode));
  emit blendingChanged(mode);
}

bool ModuleVolumeWidget::usesLighting(const int mode) const
{
  if (mode == vtkVolumeMapper::COMPOSITE_BLEND) {
    return true;
  }

  return false;
}

void ModuleVolumeWidget::setTransferMode(const int transferMode)
{
  m_ui->cbTransferMode->setCurrentIndex(transferMode);
}

void ModuleVolumeWidget::setSolidity(const double value)
{
  m_ui->soliditySlider->setValue(value);
}

void ModuleVolumeWidget::setRgbaMappingAllowed(const bool b)
{
  m_ui->useRgbaMapping->setVisible(b);

  if (!b) {
    setUseRgbaMapping(false);
  }
}

void ModuleVolumeWidget::setUseRgbaMapping(const bool b)
{
  m_ui->useRgbaMapping->setChecked(b);
}

void ModuleVolumeWidget::setRgbaMappingMin(const double v)
{
  m_ui->sliRgbaMappingMin->setValue(v);
}

void ModuleVolumeWidget::setRgbaMappingMax(const double v)
{
  m_ui->sliRgbaMappingMax->setValue(v);
}

void ModuleVolumeWidget::setRgbaMappingSliderRange(const double range[2])
{
  double min = range[0];
  double max = range[1];
  m_ui->sliRgbaMappingMin->setMinimum(min);
  m_ui->sliRgbaMappingMin->setMaximum(max);
  m_ui->sliRgbaMappingMax->setMinimum(min);
  m_ui->sliRgbaMappingMax->setMaximum(max);
}

void ModuleVolumeWidget::onRgbaMappingMinChanged(double v)
{
  double max = m_ui->sliRgbaMappingMax->value();
  if (v > max) {
    // Don't allow the min to be greater than the max.
    // This is better than modifying the ranges so the slider
    // range doesn't visually change.
    setRgbaMappingMin(max);
    v = max;
  }
  emit rgbaMappingMinChanged(v);
}

void ModuleVolumeWidget::onRgbaMappingMaxChanged(double v)
{
  double min = m_ui->sliRgbaMappingMin->value();
  if (v < min) {
    // Don't allow the max to be less than the min.
    // This is better than modifying the ranges so the slider
    // range doesn't visually change.
    setRgbaMappingMax(min);
    v = min;
  }
  emit rgbaMappingMaxChanged(v);
}

QFormLayout* ModuleVolumeWidget::formLayout()
{
  return m_ui->formLayout;
}
} // namespace tomviz
