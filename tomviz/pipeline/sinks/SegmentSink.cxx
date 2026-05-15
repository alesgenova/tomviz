/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SegmentSink.h"
#include "SegmentSinkWidget.h"

#include "Pipeline.h"
#include "data/VolumeData.h"

// Qt defines 'slots' as a macro which conflicts with Python's object.h.
// We must undef it before including any pybind11/Python headers.
#pragma push_macro("slots")
#undef slots
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "pybind11/PybindVTKTypeCaster.h"
#pragma pop_macro("slots")

#include <QCheckBox>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <vtkActor.h>
#include <vtkColorTransferFunction.h>
#include <vtkDataSetMapper.h>
#include <vtkFlyingEdges3D.h>
#include <vtkImageData.h>
#include <vtkPVRenderView.h>
#include <vtkProperty.h>
#include <vtkSMProxy.h>

PYBIND11_VTK_TYPECASTER(vtkImageData)

namespace py = pybind11;

namespace tomviz {
namespace pipeline {

// Default ITK segmentation script — same as ModuleSegment's default.
static const char* DEFAULT_SCRIPT =
  "def run_itk_segmentation(itk_image, itk_image_type):\n"
  "    # should return the result image and result image type like this:\n"
  "    # return outImage, outImageType\n"
  "    # An example segmentation script follows: \n\n"
  "    # Create a filter (ConfidenceConnectedImageFilter) for the input "
  "image type\n"
  "    itk_filter = "
  "itk.ConfidenceConnectedImageFilter[itk_image_type,itk.Image.SS3].New()\n\n"
  "    # Set input parameters on the filter (these are copied from an "
  "example in ITK.\n"
  "    itk_filter.SetInitialNeighborhoodRadius(3)\n"
  "    itk_filter.SetMultiplier(3)\n"
  "    itk_filter.SetNumberOfIterations(25)\n"
  "    itk_filter.SetReplaceValue(255)\n"
  "    itk_filter.SetSeed((24,65,37))\n\n"
  "    # Hand the input image to the filter\n"
  "    itk_filter.SetInput(itk_image)\n"
  "    # Run the filter\n"
  "    itk_filter.Update()\n\n"
  "    # Return the output and the output type (itk.Image.SS3 is one of "
  "the valid output\n"
  "    # types for this filter and is the one we specified when we created "
  "the filter above\n"
  "    return itk_filter.GetOutput(), itk.Image.SS3\n";

// Wrapper script template that embeds the user script and handles
// VTK <-> ITK conversion — matches ModuleSegment::onPropertyChanged().
static const char* WRAPPER_TEMPLATE =
  "import vtk\n"
  "from tomviz import utils\n"
  "import itk\n"
  "\n"
  "array = utils.get_array(dataset)\n"
  "original_origin = dataset.GetOrigin()\n"
  "original_extent = dataset.GetExtent()\n"
  "original_spacing = dataset.GetSpacing()\n"
  "\n"
  "itk_image_type = itk.Image.F3\n"
  "itk_converter = itk.PyBuffer[itk_image_type]\n"
  "itk_image = itk_converter.GetImageFromArray(array)\n"
  "\n"
  "%1\n"
  "\n"
  "output_itk_image, output_type = run_itk_segmentation(itk_image, "
  "itk_image_type)\n"
  "\n"
  "output_array = "
  "itk.PyBuffer[output_type].GetArrayFromImage(output_itk_image)\n"
  "utils.set_array(dataset, output_array)\n"
  "\n"
  "if array.shape == output_array.shape:\n"
  "    dataset.SetOrigin(original_origin)\n"
  "    dataset.SetExtent(original_extent)\n"
  "    dataset.SetSpacing(original_spacing)\n";

SegmentSink::SegmentSink(QObject* parent) : LegacyModuleSink(parent)
{
  addInput("volume", PortType::ImageData);
  setLabel("Segment");

  m_script = DEFAULT_SCRIPT;

  m_contour->SetValue(0, m_contourValue);
  m_property->SetRepresentationToSurface();
  m_property->SetSpecular(1.0);
  m_property->SetSpecularPower(100.0);

  m_mapper->SetInputConnection(m_contour->GetOutputPort());
  m_actor->SetMapper(m_mapper);
  m_actor->SetVisibility(0);
  m_actor->SetProperty(m_property);
}

SegmentSink::~SegmentSink()
{
  finalize();
}

QIcon SegmentSink::icon() const
{
  return QIcon(QStringLiteral(":/pqWidgets/Icons/pqCalculator.svg"));
}

void SegmentSink::setVisibility(bool visible)
{
  m_actor->SetVisibility(visible ? 1 : 0);
  LegacyModuleSink::setVisibility(visible);
}

bool SegmentSink::isColorMapNeeded() const
{
  return true;
}

bool SegmentSink::initialize(vtkSMViewProxy* view)
{
  if (!LegacyModuleSink::initialize(view)) {
    return false;
  }

  renderView()->AddPropToRenderer(m_actor);
  return true;
}

bool SegmentSink::finalize()
{
  if (renderView()) {
    renderView()->RemovePropFromRenderer(m_actor);
  }
  return LegacyModuleSink::finalize();
}

void SegmentSink::clearVisualization()
{
  m_actor->SetVisibility(0);
}

bool SegmentSink::runSegmentation(vtkImageData* input)
{
  m_segmentedData->DeepCopy(input);

  // Ensure Python is initialized. Never finalize - C extension modules
  // like numpy cannot be re-loaded after finalize_interpreter().
  if (!Py_IsInitialized()) {
    py::initialize_interpreter();
  }

  bool success = false;
  try {
    py::gil_scoped_acquire gil;

    // Build the wrapper script (embeds the user's segmentation function)
    QString wrapperScript = QString(WRAPPER_TEMPLATE).arg(m_script);

    // Create namespace and inject the DeepCopied vtkImageData as 'dataset'
    py::dict ns;
    ns["dataset"] =
      py::cast(static_cast<vtkImageData*>(m_segmentedData.Get()),
               py::return_value_policy::reference);

    py::exec(py::str(wrapperScript.toStdString()), ns);
    success = true;
  } catch (const py::error_already_set& e) {
    qWarning("SegmentSink Python error: %s", e.what());
  } catch (const std::exception& e) {
    qWarning("SegmentSink error: %s", e.what());
  }

  return success;
}

bool SegmentSink::consume(const QMap<QString, PortData>& inputs)
{
  if (!validateInput(inputs, "volume")) {
    return false;
  }

  auto volume = inputs["volume"].value<VolumeDataPtr>();
  if (!volume || !volume->isValid()) {
    return false;
  }

  auto* imageData = volume->imageData();

  // Run segmentation script; fall back to original input on failure
  vtkImageData* contourInput = imageData;
  if (!m_script.isEmpty()) {
    if (runSegmentation(imageData)) {
      contourInput = m_segmentedData;
    }
  }

  m_contour->SetInputData(contourInput);
  m_contour->SetValue(0, m_contourValue);
  m_actor->SetVisibility(visibility() ? 1 : 0);

  onMetadataChanged();
  emit renderNeeded();
  return true;
}

QString SegmentSink::script() const
{
  return m_script;
}

void SegmentSink::setScript(const QString& script)
{
  m_script = script;
}

double SegmentSink::contourValue() const
{
  return m_contourValue;
}

void SegmentSink::setContourValue(double value)
{
  m_contourValue = value;
  m_contour->SetValue(0, value);
  emit renderNeeded();
}

double SegmentSink::opacity() const
{
  return m_property->GetOpacity();
}

void SegmentSink::setOpacity(double value)
{
  m_property->SetOpacity(value);
  emit renderNeeded();
}

double SegmentSink::specular() const
{
  return m_property->GetSpecular();
}

void SegmentSink::setSpecular(double value)
{
  m_property->SetSpecular(value);
  emit renderNeeded();
}

int SegmentSink::representation() const
{
  return m_property->GetRepresentation();
}

void SegmentSink::setRepresentation(int rep)
{
  m_property->SetRepresentation(rep);
  emit renderNeeded();
}

QString SegmentSink::representationString() const
{
  return QString::fromUtf8(m_property->GetRepresentationAsString());
}

void SegmentSink::setRepresentationString(const QString& rep)
{
  if (rep == "Surface")
    m_property->SetRepresentationToSurface();
  else if (rep == "Points")
    m_property->SetRepresentationToPoints();
  else if (rep == "Wireframe")
    m_property->SetRepresentationToWireframe();

  emit renderNeeded();
}

// --- Properties Widget ---

QWidget* SegmentSink::createSinkPropertiesWidget(QWidget* parent)
{
  auto* widget = new QWidget(parent);
  auto* layout = new QVBoxLayout;
  widget->setLayout(layout);

  // --- Separate Color Map checkbox ---
  auto* separateCmapCheck = new QCheckBox("Separate Color Map", widget);
  {
    QSignalBlocker blocker(separateCmapCheck);
    separateCmapCheck->setChecked(useDetachedColorMap());
  }
  layout->addWidget(separateCmapCheck);
  connect(separateCmapCheck, &QCheckBox::toggled,
          [this](bool on) { setUseDetachedColorMap(on); });

  // Create, update and connect
  m_controllers = new SegmentSinkWidget;
  layout->addWidget(m_controllers);

  updatePanel();

  connect(m_controllers, &SegmentSinkWidget::scriptApplied, this,
          [this](const QString& s) {
            setScript(s);
            // Re-run the pipeline so the new script is executed.
            if (auto* pip = qobject_cast<Pipeline*>(QObject::parent())) {
              pip->execute();
            }
          });
  connect(m_controllers, &SegmentSinkWidget::contourValueChanged, this,
          [this](double v) { setContourValue(v); });
  connect(m_controllers, &SegmentSinkWidget::representationChanged, this,
          [this](const QString& rep) { setRepresentationString(rep); });
  connect(m_controllers, &SegmentSinkWidget::opacityChanged, this,
          [this](double v) { setOpacity(v); });
  connect(m_controllers, &SegmentSinkWidget::specularChanged, this,
          [this](double v) { setSpecular(v); });

  return widget;
}

void SegmentSink::updatePanel()
{
  if (!m_controllers)
    return;

  QSignalBlocker blocker(m_controllers);

  m_controllers->setScript(m_script);
  m_controllers->setContourValue(m_contourValue);
  m_controllers->setRepresentation(representationString());
  m_controllers->setOpacity(opacity());
  m_controllers->setSpecular(specular());
}

void SegmentSink::updateColorMap()
{
  auto* cmap = colorMap();
  if (cmap) {
    auto* ctf = vtkColorTransferFunction::SafeDownCast(
      cmap->GetClientSideObject());
    if (ctf) {
      m_mapper->SetLookupTable(ctf);
    }
  }
  emit renderNeeded();
}

// --- Clipping ---

void SegmentSink::addClippingPlane(vtkPlane* plane)
{
  if (plane) {
    m_mapper->AddClippingPlane(plane);
    emit renderNeeded();
  }
}

void SegmentSink::removeClippingPlane(vtkPlane* plane)
{
  if (plane) {
    m_mapper->RemoveClippingPlane(plane);
    emit renderNeeded();
  }
}

QJsonObject SegmentSink::serialize() const
{
  auto json = LegacyModuleSink::serialize();
  json["contourValue"] = m_contourValue;
  json["opacity"] = m_property->GetOpacity();
  json["specular"] = m_property->GetSpecular();
  json["representation"] = m_property->GetRepresentation();
  json["script"] = m_script;
  return json;
}

bool SegmentSink::deserialize(const QJsonObject& json)
{
  if (!LegacyModuleSink::deserialize(json)) {
    return false;
  }
  if (json.contains("contourValue")) {
    setContourValue(json["contourValue"].toDouble());
  }
  if (json.contains("opacity")) {
    setOpacity(json["opacity"].toDouble());
  }
  if (json.contains("specular")) {
    setSpecular(json["specular"].toDouble());
  }
  if (json.contains("representation")) {
    setRepresentation(json["representation"].toInt());
  }
  if (json.contains("script")) {
    setScript(json["script"].toString());
  }
  return true;
}

void SegmentSink::onMetadataChanged()
{
  auto vol = volumeData();
  if (!vol) return;
  auto pos = vol->displayPosition();
  auto orient = vol->displayOrientation();
  m_actor->SetPosition(pos.data());
  m_actor->SetOrientation(orient.data());
  emit renderNeeded();
}

} // namespace pipeline
} // namespace tomviz
