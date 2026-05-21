/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ManualManipulationWidget.h"
#include "ui_ManualManipulationWidget.h"

#include "pipeline/PortType.h"
#include "pipeline/data/VolumeData.h"

#include <vtkImageData.h>

#include <cmath>

namespace tomviz {

class ManualManipulationWidget::Internal
{
public:
  Ui::ManualManipulationWidget ui;
  vtkSmartPointer<vtkImageData> image;

  Internal(vtkSmartPointer<vtkImageData> img, ManualManipulationWidget* parent)
    : image(img)
  {
    ui.setupUi(parent);

    // Hide groups that require DataSource/ModuleManager infrastructure
    ui.interactionGroup->hide();
    ui.referenceDataGroup->hide();

    // Initialize scale spinboxes from image spacing
    if (image) {
      double spacing[3];
      image->GetSpacing(spacing);
      ui.scaleX->setValue(spacing[0]);
      ui.scaleY->setValue(spacing[1]);
      ui.scaleZ->setValue(spacing[2]);
    }
  }

  QList<QVariant> scaling() const
  {
    return { ui.scaleX->value(), ui.scaleY->value(), ui.scaleZ->value() };
  }

  QList<QVariant> rotation() const
  {
    return { ui.rotateX->value(), ui.rotateY->value(), ui.rotateZ->value() };
  }

  QList<QVariant> physicalShift() const
  {
    return { ui.shiftX->value(), ui.shiftY->value(), ui.shiftZ->value() };
  }

  QList<QVariant> voxelShift() const
  {
    if (!image) {
      return { 0, 0, 0 };
    }

    double spacing[3];
    image->GetSpacing(spacing);
    const int* dims = image->GetDimensions();

    int shifts[3];
    for (int i = 0; i < 3; ++i) {
      double length = spacing[i] * dims[i];
      double physVal = physicalShift()[i].toDouble();
      shifts[i] =
        (length > 0) ? static_cast<int>(std::round(physVal / length * dims[i]))
                     : 0;
    }

    return { shifts[0], shifts[1], shifts[2] };
  }

  void setScaling(const double* scale)
  {
    ui.scaleX->setValue(scale[0]);
    ui.scaleY->setValue(scale[1]);
    ui.scaleZ->setValue(scale[2]);
  }

  void setRotation(const double* rot)
  {
    ui.rotateX->setValue(rot[0]);
    ui.rotateY->setValue(rot[1]);
    ui.rotateZ->setValue(rot[2]);
  }

  void setVoxelShift(const int* shift)
  {
    if (!image) {
      return;
    }

    double spacing[3];
    image->GetSpacing(spacing);
    const int* dims = image->GetDimensions();

    for (int i = 0; i < 3; ++i) {
      double length = spacing[i] * dims[i];
      double physVal = (dims[i] > 0) ? shift[i] * length / dims[i] : 0.0;
      QList<QDoubleSpinBox*> widgets = { ui.shiftX, ui.shiftY, ui.shiftZ };
      widgets[i]->setValue(physVal);
    }
  }

  QList<QVariant> referenceSpacing() const
  {
    if (image) {
      double spacing[3];
      image->GetSpacing(spacing);
      return { spacing[0], spacing[1], spacing[2] };
    }
    return { 1.0, 1.0, 1.0 };
  }

  QList<QVariant> referenceShape() const
  {
    if (image) {
      const int* dims = image->GetDimensions();
      return { dims[0], dims[1], dims[2] };
    }
    return { 1, 1, 1 };
  }
};

ManualManipulationWidget::ManualManipulationWidget(
  const QMap<QString, pipeline::PortData>& inputs, QWidget* parent)
  : CustomPythonNodeWidget(parent)
{
  vtkSmartPointer<vtkImageData> image;
  if (auto it = inputs.constFind(QStringLiteral("volume"));
      it != inputs.constEnd()) {
    const auto& portData = it.value();
    if (portData.isValid() && pipeline::isVolumeType(portData.type())) {
      if (auto vol = portData.value<pipeline::VolumeDataPtr>();
          vol && vol->isValid()) {
        image = vol->imageData();
      }
    }
  }
  m_internal.reset(new Internal(image, this));
}

ManualManipulationWidget::~ManualManipulationWidget() = default;

void ManualManipulationWidget::getValues(QMap<QString, QVariant>& map)
{
  map.insert("scaling", m_internal->scaling());
  map.insert("rotation", m_internal->rotation());
  map.insert("shift", m_internal->voxelShift());
  map.insert("align_with_reference", false);
  map.insert("reference_spacing", m_internal->referenceSpacing());
  map.insert("reference_shape", m_internal->referenceShape());
}

void ManualManipulationWidget::setValues(const QMap<QString, QVariant>& map)
{
  if (map.contains("scaling")) {
    auto array = map["scaling"].toList();
    if (array.size() >= 3) {
      double scaling[3] = { array[0].toDouble(), array[1].toDouble(),
                            array[2].toDouble() };
      m_internal->setScaling(scaling);
    }
  }

  if (map.contains("rotation")) {
    auto array = map["rotation"].toList();
    if (array.size() >= 3) {
      double rotation[3] = { array[0].toDouble(), array[1].toDouble(),
                             array[2].toDouble() };
      m_internal->setRotation(rotation);
    }
  }

  if (map.contains("shift")) {
    auto array = map["shift"].toList();
    if (array.size() >= 3) {
      int shift[3] = { array[0].toInt(), array[1].toInt(),
                        array[2].toInt() };
      m_internal->setVoxelShift(shift);
    }
  }
}

} // namespace tomviz
