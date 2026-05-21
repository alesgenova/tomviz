/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TransposeDataTransform.h"

#include "EditNodeWidget.h"
#include "data/VolumeData.h"

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QVBoxLayout>

namespace {

class TransposeDataWidget : public tomviz::pipeline::EditNodeWidget
{
  Q_OBJECT

public:
  TransposeDataWidget(tomviz::pipeline::TransposeDataTransform* op,
                      QWidget* p)
    : tomviz::pipeline::EditNodeWidget(p), m_operator(op),
      m_transposeTypesCombo(nullptr)
  {
    auto* transposeLabel = new QLabel("Transpose to:", this);
    transposeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_transposeTypesCombo = new QComboBox(this);
    using TransposeType =
      tomviz::pipeline::TransposeDataTransform::TransposeType;
    m_transposeTypesCombo->insertItem(static_cast<int>(TransposeType::C),
                                      "C Ordering");
    m_transposeTypesCombo->insertItem(static_cast<int>(TransposeType::Fortran),
                                      "Fortran Ordering");

    auto* vBoxLayout = new QVBoxLayout(this);

    auto* convertHBoxLayout = new QHBoxLayout;
    convertHBoxLayout->addWidget(transposeLabel);
    convertHBoxLayout->addWidget(m_transposeTypesCombo);
    vBoxLayout->addLayout(convertHBoxLayout);

    setLayout(vBoxLayout);
  }

  void applyChangesToOperator() override
  {
    using TransposeType =
      tomviz::pipeline::TransposeDataTransform::TransposeType;
    m_operator->setTransposeType(
      static_cast<TransposeType>(m_transposeTypesCombo->currentIndex()));
  }

private:
  QPointer<tomviz::pipeline::TransposeDataTransform> m_operator;
  QComboBox* m_transposeTypesCombo;
};

} // namespace

#include "TransposeDataTransform.moc"

namespace {

template <typename T>
void ReorderArrayC(T* in, T* out, int dim[3])
{
  for (int i = 0; i < dim[0]; ++i) {
    for (int j = 0; j < dim[1]; ++j) {
      for (int k = 0; k < dim[2]; ++k) {
        out[static_cast<size_t>(i * dim[1] + j) * dim[2] + k] =
          in[static_cast<size_t>(k * dim[1] + j) * dim[0] + i];
      }
    }
  }
}

template <typename T>
void ReorderArrayF(T* in, T* out, int dim[3])
{
  for (int i = 0; i < dim[0]; ++i) {
    for (int j = 0; j < dim[1]; ++j) {
      for (int k = 0; k < dim[2]; ++k) {
        out[static_cast<size_t>(k * dim[1] + j) * dim[0] + i] =
          in[static_cast<size_t>(i * dim[1] + j) * dim[2] + k];
      }
    }
  }
}

} // namespace

namespace tomviz {
namespace pipeline {

TransposeDataTransform::TransposeDataTransform(QObject* parent)
  : TransformNode(parent)
{
  addInput("volume", PortType::ImageData);
  addOutput("output", PortType::ImageData);
  setLabel("Transpose Data");
}

bool TransposeDataTransform::hasPropertiesWidget() const
{
  return true;
}

EditNodeWidget* TransposeDataTransform::createPropertiesWidget(
  Pipeline* /*pipeline*/, QWidget* parent)
{
  return new TransposeDataWidget(this, parent);
}

QJsonObject TransposeDataTransform::serialize() const
{
  auto json = TransformNode::serialize();
  json["transposeType"] = (m_transposeType == TransposeType::Fortran)
                            ? QStringLiteral("Fortran")
                            : QStringLiteral("C");
  return json;
}

bool TransposeDataTransform::deserialize(const QJsonObject& json)
{
  if (!TransformNode::deserialize(json)) {
    return false;
  }
  if (json.contains("transposeType")) {
    setTransposeType(json.value("transposeType").toString() ==
                         QStringLiteral("Fortran")
                       ? TransposeType::Fortran
                       : TransposeType::C);
  }
  return true;
}

QMap<QString, PortData> TransposeDataTransform::transform(
  const QMap<QString, PortData>& inputs)
{
  QMap<QString, PortData> result;

  auto inputVolume = inputs["volume"].value<VolumeDataPtr>();
  if (!inputVolume || !inputVolume->isValid()) {
    return result;
  }

  auto* inputImage = inputVolume->imageData();
  int dim[3];
  inputImage->GetDimensions(dim);

  auto* scalars = inputImage->GetPointData()->GetScalars();
  auto* dataPtr = scalars->GetVoidPointer(0);

  vtkNew<vtkImageData> reorderedImage;
  reorderedImage->SetDimensions(dim);
  reorderedImage->AllocateScalars(scalars->GetDataType(),
                                  scalars->GetNumberOfComponents());

  auto* outputArray = reorderedImage->GetPointData()->GetScalars();
  outputArray->SetName(scalars->GetName());
  auto* outPtr = outputArray->GetVoidPointer(0);

  switch (m_transposeType) {
    case TransposeType::C:
      switch (scalars->GetDataType()) {
        vtkTemplateMacro(ReorderArrayC(
          reinterpret_cast<VTK_TT*>(dataPtr),
          reinterpret_cast<VTK_TT*>(outPtr), dim));
      }
      break;
    case TransposeType::Fortran:
      switch (scalars->GetDataType()) {
        vtkTemplateMacro(ReorderArrayF(
          reinterpret_cast<VTK_TT*>(dataPtr),
          reinterpret_cast<VTK_TT*>(outPtr), dim));
      }
      break;
  }

  // Copy the rest of the image metadata and replace scalars
  reorderedImage->SetSpacing(inputImage->GetSpacing());
  reorderedImage->SetOrigin(inputImage->GetOrigin());

  auto volume = std::make_shared<VolumeData>(reorderedImage.Get());
  volume->setLabel(inputVolume->label());
  volume->setUnits(inputVolume->units());
  volume->setTiltAngles(inputVolume->tiltAngles());

  result["output"] = PortData(std::any(volume), inputs["volume"].type());
  return result;
}

} // namespace pipeline
} // namespace tomviz
