/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ArrayWranglerTransform.h"

#include "EditNodeWidget.h"
#include "GatedEditorWidget.h"
#include "InputPort.h"
#include "data/VolumeData.h"

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkTypeUInt16Array.h>
#include <vtkTypeUInt8Array.h>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QVBoxLayout>

namespace {

class ArrayWranglerWidget : public tomviz::pipeline::EditNodeWidget
{
  Q_OBJECT

public:
  ArrayWranglerWidget(tomviz::pipeline::ArrayWranglerTransform* op,
                      tomviz::pipeline::VolumeDataPtr volumeData, QWidget* p)
    : tomviz::pipeline::EditNodeWidget(p), m_operator(op),
      m_outputTypesCombo(nullptr), m_componentToKeepCombo(nullptr)
  {
    auto* convertLabel = new QLabel("Convert to:", this);
    convertLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_outputTypesCombo = new QComboBox(this);
    using OutputType = tomviz::pipeline::ArrayWranglerTransform::OutputType;
    m_outputTypesCombo->insertItem(static_cast<int>(OutputType::UInt8),
                                   "UInt8");
    m_outputTypesCombo->insertItem(static_cast<int>(OutputType::UInt16),
                                   "UInt16");

    auto* vBoxLayout = new QVBoxLayout(this);

    auto* convertHBoxLayout = new QHBoxLayout;
    convertHBoxLayout->addWidget(convertLabel);
    convertHBoxLayout->addWidget(m_outputTypesCombo);
    vBoxLayout->addLayout(convertHBoxLayout);

    auto numComponents =
      volumeData->imageData()->GetPointData()->GetScalars()
        ->GetNumberOfComponents();
    if (numComponents > 1) {
      auto* numComponentsLabel = new QLabel("Component to Keep:");
      numComponentsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

      m_componentToKeepCombo = new QComboBox(this);
      for (int i = 1; i < numComponents + 1; ++i)
        m_componentToKeepCombo->addItem(QString::number(i));

      auto* componentHBoxLayout = new QHBoxLayout;
      componentHBoxLayout->addWidget(numComponentsLabel);
      componentHBoxLayout->addWidget(m_componentToKeepCombo);
      vBoxLayout->addLayout(componentHBoxLayout);
    }

    setLayout(vBoxLayout);
  }

  void applyChangesToOperator() override
  {
    using OutputType = tomviz::pipeline::ArrayWranglerTransform::OutputType;
    m_operator->setOutputType(
      static_cast<OutputType>(m_outputTypesCombo->currentIndex()));

    if (m_componentToKeepCombo)
      m_operator->setComponentToKeep(m_componentToKeepCombo->currentIndex());
    else
      m_operator->setComponentToKeep(0);
  }

private:
  QPointer<tomviz::pipeline::ArrayWranglerTransform> m_operator;
  QComboBox* m_outputTypesCombo;
  QComboBox* m_componentToKeepCombo;
};

} // namespace

#include "ArrayWranglerTransform.moc"

namespace {

template <typename vtkInputDataType, typename vtkOutputArrayType,
          typename = std::enable_if<std::is_unsigned<
            typename vtkOutputArrayType::ValueType>::value>>
void wrangleVtkArrayTypeUnsigned(vtkOutputArrayType* array, int nComps,
                                 int componentToKeep, vtkIdType nTuples,
                                 void* data, double oldrange[2])
{
  // We can't divide by zero...
  // assert(oldrange[1] != oldrange[0]);

  // GetDataTypeValueMax() is supposed to return the native data type
  auto newmax = array->GetDataTypeValueMax();
  using outputType = decltype(newmax);

  double multiplier = newmax / (oldrange[1] - oldrange[0]);

  auto d = static_cast<vtkInputDataType*>(data);
  auto a = static_cast<outputType*>(array->GetVoidPointer(0));
  for (vtkIdType i = 0; i < nTuples; ++i) {
    a[i] = static_cast<outputType>(
      (d[i * nComps + componentToKeep] - oldrange[0]) * multiplier + 0.5);
  }
}

template <typename vtkOutputArrayType>
void applyGenericTransform(vtkImageData* imageData, int componentToKeep)
{
  auto scalars = imageData->GetPointData()->GetScalars();

  double range[2];
  scalars->GetFiniteRange(range);

  vtkNew<vtkOutputArrayType> outputArray;
  outputArray->SetNumberOfComponents(1);
  outputArray->SetNumberOfTuples(scalars->GetNumberOfTuples());
  outputArray->SetName(scalars->GetName());

  switch (scalars->GetDataType()) {
    vtkTemplateMacro(wrangleVtkArrayTypeUnsigned<VTK_TT>(
      outputArray.Get(), scalars->GetNumberOfComponents(), componentToKeep,
      scalars->GetNumberOfTuples(), scalars->GetVoidPointer(0), range));
  }

  imageData->GetPointData()->RemoveArray(scalars->GetName());
  imageData->GetPointData()->SetScalars(outputArray);
}

} // namespace

namespace tomviz {
namespace pipeline {

ArrayWranglerTransform::ArrayWranglerTransform(QObject* parent)
  : TransformNode(parent)
{
  addInput("volume", PortType::ImageData);
  addOutput("output", PortType::ImageData);
  setLabel("Convert Type");
}

bool ArrayWranglerTransform::hasPropertiesWidget() const
{
  return true;
}

EditNodeWidget* ArrayWranglerTransform::createPropertiesWidget(
  Pipeline* pipeline, QWidget* parent)
{
  return new GatedEditorWidget(
    this, pipeline,
    [this](QWidget* p) -> EditNodeWidget* {
      auto vol = inputPorts()[0]->data().value<VolumeDataPtr>();
      return new ArrayWranglerWidget(this, vol, p);
    },
    parent);
}

QJsonObject ArrayWranglerTransform::serialize() const
{
  auto json = TransformNode::serialize();
  json["outputType"] = (m_outputType == OutputType::UInt16)
                         ? QStringLiteral("UInt16")
                         : QStringLiteral("UInt8");
  json["componentToKeep"] = m_componentToKeep;
  return json;
}

bool ArrayWranglerTransform::deserialize(const QJsonObject& json)
{
  if (!TransformNode::deserialize(json)) {
    return false;
  }
  if (json.contains("outputType")) {
    setOutputType(json.value("outputType").toString() ==
                      QStringLiteral("UInt16")
                    ? OutputType::UInt16
                    : OutputType::UInt8);
  }
  if (json.contains("componentToKeep")) {
    setComponentToKeep(json.value("componentToKeep").toInt());
  }
  return true;
}

QMap<QString, PortData> ArrayWranglerTransform::transform(
  const QMap<QString, PortData>& inputs)
{
  QMap<QString, PortData> result;

  auto inputVolume = inputs["volume"].value<VolumeDataPtr>();
  if (!inputVolume || !inputVolume->isValid()) {
    return result;
  }

  auto* inputImage = inputVolume->imageData();
  auto* scalars = inputImage->GetPointData()->GetScalars();
  if (!scalars) {
    return result;
  }

  if (m_componentToKeep >= scalars->GetNumberOfComponents()) {
    return result;
  }

  vtkNew<vtkImageData> outputImage;
  outputImage->DeepCopy(inputImage);

  switch (m_outputType) {
    case OutputType::UInt8:
      applyGenericTransform<vtkTypeUInt8Array>(outputImage, m_componentToKeep);
      break;
    case OutputType::UInt16:
      applyGenericTransform<vtkTypeUInt16Array>(outputImage, m_componentToKeep);
      break;
  }

  auto volume = std::make_shared<VolumeData>(outputImage.Get());
  volume->setLabel(inputVolume->label());
  volume->setUnits(inputVolume->units());
  volume->setTiltAngles(inputVolume->tiltAngles());

  result["output"] = PortData(std::any(volume), PortType::ImageData);
  return result;
}

} // namespace pipeline
} // namespace tomviz
