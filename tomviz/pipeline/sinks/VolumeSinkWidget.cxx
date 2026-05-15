/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "VolumeSinkWidget.h"
#include "ui_LightingParametersForm.h"
#include "ui_VolumeSinkWidget.h"

#include "vtkVolumeMapper.h"

namespace tomviz {

// If we make this bigger, such as 1000, and we make the max too
// close to the data minimum or the min too close to the data maximum,
// we run into errors like these:
// ( 118.718s) [paraview        ]vtkOpenGLVolumeLookupTa:84    WARN|
// vtkOpenGLVolumeRGBTable (0x55ba0c5cc970): This OpenGL implementation does not
// support the required texture size of 65536, falling back to maximum allowed,
// 32768.This may cause an incorrect lookup table mapping.
static const double RANGE_INCREMENT = 500;

VolumeSinkWidget::VolumeSinkWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::VolumeSinkWidget),
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

  connect(m_ui->cbJittering, &QCheckBox::toggled, this,
          &VolumeSinkWidget::jitteringToggled);
  connect(m_ui->cbBlending, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &VolumeSinkWidget::onBlendingChanged);
  connect(m_ui->cbInterpolation,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &VolumeSinkWidget::interpolationChanged);
  connect(m_ui->cbTransferMode,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &VolumeSinkWidget::transferModeChanged);
  connect(m_ui->cbMultiVolume, &QCheckBox::toggled, this,
          &VolumeSinkWidget::allowMultiVolumeToggled);
  connect(m_ui->cbMultiVolume, &QCheckBox::toggled, this,
          &VolumeSinkWidget::setAllowMultiVolume);

  connect(m_ui->useRgbaMapping, &QCheckBox::toggled, this,
          &VolumeSinkWidget::useRgbaMappingToggled);

  connect(m_ui->rgbaMappingCombineComponents, &QCheckBox::toggled, this,
          &VolumeSinkWidget::rgbaMappingCombineComponentsToggled);
  connect(m_ui->rgbaMappingComponent, &QComboBox::currentTextChanged, this,
          &VolumeSinkWidget::rgbaMappingComponentChanged);

  // Using QueuedConnections here to circumvent DoubleSliderWidget->BlockUpdate
  connect(m_ui->sliRgbaMappingMin, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::onRgbaMappingMinChanged, Qt::QueuedConnection);
  connect(m_ui->sliRgbaMappingMax, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::onRgbaMappingMaxChanged, Qt::QueuedConnection);

  connect(m_uiLighting->gbLighting, &QGroupBox::toggled, this,
          &VolumeSinkWidget::lightingToggled);
  connect(m_uiLighting->sliAmbient, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::ambientChanged);
  connect(m_uiLighting->sliDiffuse, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::diffuseChanged);
  connect(m_uiLighting->sliSpecular, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::specularChanged);
  connect(m_uiLighting->sliSpecularPower, &DoubleSliderWidget::valueEdited,
          this, &VolumeSinkWidget::specularPowerChanged);
  connect(m_ui->soliditySlider, &DoubleSliderWidget::valueEdited, this,
          &VolumeSinkWidget::solidityChanged);

  // TODO: multi-volume rendering is not yet implemented for VolumeSink.
  // The legacy VolumeManager only works with ModuleVolume. When re-enabling,
  // the per-volume scalar array selection must be fixed: the shared
  // vtkGPUVolumeRayCastMapper in VolumeManager doesn't propagate each
  // volume's SelectScalarArray() call, so multiple volumes on the same
  // dataset all render whichever scalar was last set as active on the
  // shared vtkImageData. Fix by giving each volume port a shallow-copied
  // vtkImageData with the correct active scalar, or by using per-port
  // SetInputArrayToProcess() on the shared mapper.
  m_ui->cbMultiVolume->setVisible(false);
  m_ui->groupRgbaMappingRange->setVisible(false);
  m_ui->rgbaMappingComponentLabel->setVisible(false);
  m_ui->rgbaMappingComponent->setVisible(false);
}

VolumeSinkWidget::~VolumeSinkWidget() = default;

void VolumeSinkWidget::setJittering(const bool enable)
{
  m_ui->cbJittering->setChecked(enable);
}

void VolumeSinkWidget::setBlendingMode(const int mode)
{
  m_uiLighting->gbLighting->setEnabled(usesLighting(mode));
  m_ui->cbBlending->setCurrentIndex(static_cast<int>(mode));
}

void VolumeSinkWidget::setInterpolationType(const int type)
{
  m_ui->cbInterpolation->setCurrentIndex(type);
}

void VolumeSinkWidget::setLighting(const bool enable)
{
  m_uiLighting->gbLighting->setChecked(enable);
}

void VolumeSinkWidget::setAmbient(const double value)
{
  m_uiLighting->sliAmbient->setValue(value);
}

void VolumeSinkWidget::setDiffuse(const double value)
{
  m_uiLighting->sliDiffuse->setValue(value);
}

void VolumeSinkWidget::setSpecular(const double value)
{
  m_uiLighting->sliSpecular->setValue(value);
}

void VolumeSinkWidget::setSpecularPower(const double value)
{
  m_uiLighting->sliSpecularPower->setValue(value);
}

void VolumeSinkWidget::onBlendingChanged(const int mode)
{
  m_uiLighting->gbLighting->setEnabled(usesLighting(mode));
  emit blendingChanged(mode);
}

bool VolumeSinkWidget::usesLighting(const int mode) const
{
  if (mode == vtkVolumeMapper::COMPOSITE_BLEND) {
    return true;
  }

  return false;
}

void VolumeSinkWidget::setTransferMode(const int transferMode)
{
  m_ui->cbTransferMode->setCurrentIndex(transferMode);
}

void VolumeSinkWidget::setSolidity(const double value)
{
  m_ui->soliditySlider->setValue(value);
}

void VolumeSinkWidget::setRgbaMappingAllowed(const bool b)
{
  m_ui->useRgbaMapping->setVisible(b);

  if (!b) {
    setUseRgbaMapping(false);
  }
}

void VolumeSinkWidget::setUseRgbaMapping(const bool b)
{
  m_ui->useRgbaMapping->setChecked(b);
}

void VolumeSinkWidget::setRgbaMappingMin(const double v)
{
  m_ui->sliRgbaMappingMin->setValue(v);
}

void VolumeSinkWidget::setRgbaMappingMax(const double v)
{
  m_ui->sliRgbaMappingMax->setValue(v);
}

void VolumeSinkWidget::setRgbaMappingSliderRange(const double range[2])
{
  double min = range[0];
  double max = range[1];
  m_ui->sliRgbaMappingMin->setMinimum(min);
  m_ui->sliRgbaMappingMin->setMaximum(max);
  m_ui->sliRgbaMappingMax->setMinimum(min);
  m_ui->sliRgbaMappingMax->setMaximum(max);
}

void VolumeSinkWidget::setRgbaMappingCombineComponents(const bool b)
{
  m_ui->rgbaMappingCombineComponents->setChecked(b);
  m_ui->rgbaMappingComponent->setVisible(!b);
  m_ui->rgbaMappingComponentLabel->setVisible(!b);
}

void VolumeSinkWidget::setRgbaMappingComponentOptions(
  const QStringList& components)
{
  m_ui->rgbaMappingComponent->clear();
  m_ui->rgbaMappingComponent->addItems(components);
}

void VolumeSinkWidget::setRgbaMappingComponent(const QString& component)
{
  m_ui->rgbaMappingComponent->setCurrentText(component);
}

void VolumeSinkWidget::setAllowMultiVolume(const bool checked)
{
  if (checked != m_ui->cbMultiVolume->isChecked()) {
    m_ui->cbMultiVolume->setChecked(checked);
  }

  m_uiLighting->gbLighting->setEnabled(!checked ||
                                       !m_ui->cbMultiVolume->isEnabled());
}

void VolumeSinkWidget::setEnableAllowMultiVolume(const bool enable)
{
  if (enable != m_ui->cbMultiVolume->isEnabled()) {
    m_ui->cbMultiVolume->setEnabled(enable);
  }

  m_uiLighting->gbLighting->setEnabled(!enable ||
                                       !m_ui->cbMultiVolume->isChecked());
}

void VolumeSinkWidget::onRgbaMappingMinChanged(double v)
{
  // Compute an increment. Don't let the min value get closer
  // than this to the maximum.
  double fullRange[2] = { m_ui->sliRgbaMappingMax->minimum(),
                          m_ui->sliRgbaMappingMax->maximum() };
  double increment = (fullRange[1] - fullRange[0]) / RANGE_INCREMENT;
  double trueMaximum = fullRange[1] - increment;
  if (v > trueMaximum) {
    setRgbaMappingMin(trueMaximum);
    v = trueMaximum;
  }

  double currentMax = m_ui->sliRgbaMappingMax->value();
  if (v > currentMax) {
    // Set the maximum to be an increment above...
    setRgbaMappingMax(v + increment);
  }

  emit rgbaMappingMinChanged(v);
}

void VolumeSinkWidget::onRgbaMappingMaxChanged(double v)
{
  // Compute an increment. Don't let the max value get closer
  // than this to the minimum.
  double fullRange[2] = { m_ui->sliRgbaMappingMin->minimum(),
                          m_ui->sliRgbaMappingMin->maximum() };
  double increment = (fullRange[1] - fullRange[0]) / RANGE_INCREMENT;
  double trueMinimum = fullRange[0] + increment;
  if (v < trueMinimum) {
    setRgbaMappingMax(trueMinimum);
    v = trueMinimum;
  }

  double currentMin = m_ui->sliRgbaMappingMin->value();
  if (v < currentMin) {
    // Set the minimum to be an increment below...
    setRgbaMappingMin(v - increment);
  }

  emit rgbaMappingMaxChanged(v);
}

QFormLayout* VolumeSinkWidget::formLayout()
{
  return m_ui->formLayout;
}
} // namespace tomviz
