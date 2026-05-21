/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SegmentSinkWidget.h"
#include "ui_SegmentSinkWidget.h"

#include "DoubleSliderWidget.h"

namespace tomviz {

SegmentSinkWidget::SegmentSinkWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::SegmentSinkWidget)
{
  m_ui->setupUi(this);

  qobject_cast<QBoxLayout*>(QWidget::layout())->addStretch();

  const int leWidth = 50;
  m_ui->sliContourValue->setLineEditWidth(leWidth);
  m_ui->sliOpacity->setLineEditWidth(leWidth);
  m_ui->sliSpecular->setLineEditWidth(leWidth);

  m_ui->sliContourValue->setSliderTracking(false);
  m_ui->sliContourValue->setKeyboardTracking(false);

  QStringList labelsRepre;
  labelsRepre << tr("Surface") << tr("Wireframe") << tr("Points");

  for (const auto& label : labelsRepre)
    m_ui->cbRepresentation->addItem(label, label);

  connect(m_ui->btnApplyScript, &QPushButton::clicked, this,
          [this]() { emit scriptApplied(m_ui->txtScript->toPlainText()); });
  connect(m_ui->sliContourValue, &DoubleSliderWidget::valueEdited, this,
          &SegmentSinkWidget::contourValueChanged);
  connect(m_ui->cbRepresentation,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &SegmentSinkWidget::onRepresentationIndexChanged);
  connect(m_ui->sliOpacity, &DoubleSliderWidget::valueEdited, this,
          &SegmentSinkWidget::opacityChanged);
  connect(m_ui->sliSpecular, &DoubleSliderWidget::valueEdited, this,
          &SegmentSinkWidget::specularChanged);
}

SegmentSinkWidget::~SegmentSinkWidget() = default;

void SegmentSinkWidget::setScript(const QString& script)
{
  m_ui->txtScript->setPlainText(script);
}

void SegmentSinkWidget::setContourValue(double value)
{
  m_ui->sliContourValue->setValue(value);
}

void SegmentSinkWidget::setContourRange(double range[2])
{
  m_ui->sliContourValue->setMinimum(range[0]);
  m_ui->sliContourValue->setMaximum(range[1]);
}

void SegmentSinkWidget::setRepresentation(const QString& representation)
{
  for (int i = 0; i < m_ui->cbRepresentation->count(); ++i) {
    if (m_ui->cbRepresentation->itemData(i).toString() == representation) {
      m_ui->cbRepresentation->setCurrentIndex(i);
      return;
    }
  }
}

void SegmentSinkWidget::setOpacity(double value)
{
  m_ui->sliOpacity->setValue(value);
}

void SegmentSinkWidget::setSpecular(double value)
{
  m_ui->sliSpecular->setValue(value);
}

void SegmentSinkWidget::onRepresentationIndexChanged(int i)
{
  emit representationChanged(m_ui->cbRepresentation->itemData(i).toString());
}

} // namespace tomviz
