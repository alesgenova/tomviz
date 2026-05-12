/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "HistogramWidget.h"

#include "ActiveObjects.h"
#include "BrightnessContrastWidget.h"
#include "ColorMap.h"
#include "ColorMapSettingsWidget.h"
#include "DoubleSliderWidget.h"
#include "PresetDialog.h"
#include "QVTKGLWidget.h"
#include "Utilities.h"
#include "pipeline/InputPort.h"
#include "pipeline/Link.h"
#include "pipeline/Node.h"
#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PortType.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/sinks/ContourSink.h"

#include "vtkChartHistogramColorOpacityEditor.h"

#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkControlPointsItem.h>
#include <vtkDataArray.h>
#include <vtkDiscretizableColorTransferFunction.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkImageData.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderWindow.h>
#include <vtkTable.h>
#include <vtkVector.h>

#include <pqApplicationCore.h>
#include <pqSettings.h>
#include <pqView.h>

#include <vtkSMPropertyHelper.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMTransferFunctionProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkType.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>
#include <set>

#include <QDebug>

namespace tomviz {

HistogramWidget::HistogramWidget(QWidget* parent)
  : QWidget(parent), m_qvtk(new QVTKGLWidget(this))
{
  // Set up our little chart.
  m_histogramView->SetRenderWindow(m_qvtk->renderWindow());
  m_histogramView->SetInteractor(m_qvtk->interactor());
  m_histogramView->GetScene()->AddItem(m_histogramColorOpacityEditor);

  // Connect events from the histogram color/opacity editor.
  m_eventLink->Connect(m_histogramColorOpacityEditor,
                       vtkCommand::CursorChangedEvent, this,
                       SLOT(histogramClicked(vtkObject*)));
  m_eventLink->Connect(m_histogramColorOpacityEditor,
                       vtkCommand::EndEvent, this,
                       SLOT(onScalarOpacityFunctionChanged()));
  m_eventLink->Connect(m_histogramColorOpacityEditor,
                       vtkControlPointsItem::CurrentPointEditEvent, this,
                       SLOT(onCurrentPointEditEvent()));

  auto hLayout = new QHBoxLayout(this);
  hLayout->addWidget(m_qvtk);
  auto vLayout = new QVBoxLayout;
  hLayout->addLayout(vLayout);
  hLayout->setContentsMargins(0, 0, 5, 0);

  vLayout->setContentsMargins(0, 0, 0, 0);
  vLayout->addStretch(1);

  auto button = new QToolButton;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqResetRange.svg"));
  button->setToolTip("Reset data range");
  connect(button, &QToolButton::clicked, this, &HistogramWidget::onResetRangeClicked);
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/icons/pqResetRangeCustom.png"));
  button->setToolTip("Specify data range");
  connect(button, &QToolButton::clicked, this, &HistogramWidget::onCustomRangeClicked);
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqInvert.svg"));
  button->setToolTip("Invert color map");
  connect(button, &QToolButton::clicked, this, &HistogramWidget::onInvertClicked);
  vLayout->addWidget(button);

  button = new QToolButton;
  m_colorMapSettingsButton = button;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqAdvanced.svg"));
  button->setToolTip("Edit color map settings");
  connect(button, &QToolButton::clicked, this,
          &HistogramWidget::onColorMapSettingsClicked);
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqFavorites.svg"));
  button->setToolTip("Choose preset color map");
  connect(button, &QToolButton::clicked, this, &HistogramWidget::onPresetClicked);
  vLayout->addWidget(button);

  button = new QToolButton;
  m_savePresetButton = button;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqSave.svg"));
  button->setToolTip("Save current color map as a preset");
  button->setEnabled(false);
  connect(button, &QToolButton::clicked, this, &HistogramWidget::onSaveToPresetClicked);
  vLayout->addWidget(button);

  button = new QToolButton;
  m_colorLegendToolButton = button;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqScalarBar.svg"));
  button->setToolTip("Show color legend in the 3D window");
  button->setEnabled(false);
  button->setCheckable(true);
  connect(button, &QToolButton::toggled, this,
          &HistogramWidget::colorLegendToggled);
  button->setChecked(false);
  vLayout->addWidget(button);

  button = new QToolButton;
  m_brightnessAndContrastButton = button;
  button->setIcon(QIcon(":/icons/greybar.png"));
  button->setToolTip("Brightness and Contrast");
  button->setEnabled(false);
  connect(button, &QToolButton::clicked, this,
          &HistogramWidget::onBrightnessAndContrastClicked);
  vLayout->addWidget(button);

  vLayout->addStretch(1);

  connect(&ActiveObjects::instance(),
          QOverload<vtkSMViewProxy*>::of(&ActiveObjects::viewChanged),
          this, [this](vtkSMViewProxy*) { updateUI(); });
  // TODO: activeNodeChanged provides pipeline::Node*, need to extract
  // DataSource context for updateColorMapDialogs when needed.
  connect(&ActiveObjects::instance(),
          &ActiveObjects::activeNodeChanged, this,
          [this](pipeline::Node*) { updateColorMapDialogs(); });
  // TODO: ModuleManager::dataSourceRemoved signal no longer available.
  // Need to listen for node removal from the pipeline instead.
  // connect to pipeline's nodeRemoved signal when available.
  connect(this, &HistogramWidget::colorMapUpdated, this, &HistogramWidget::updateUI);

  setLayout(hLayout);
}

HistogramWidget::~HistogramWidget() = default;

void HistogramWidget::setLUT(vtkDiscretizableColorTransferFunction* lut)
{
  if (m_LUT != lut) {
    if (m_LUT) {
      m_eventLink->Disconnect(m_LUT, vtkCommand::ModifiedEvent, this,
                              SLOT(onColorFunctionChanged()));
    }
    if (m_scalarOpacityFunction) {
      m_eventLink->Disconnect(m_scalarOpacityFunction,
                              vtkCommand::ModifiedEvent, this,
                              SLOT(onScalarOpacityFunctionChanged()));
    }
    m_LUT = lut;
    m_scalarOpacityFunction = m_LUT->GetScalarOpacityFunction();

    m_eventLink->Connect(m_LUT, vtkCommand::ModifiedEvent, this,
                         SLOT(onColorFunctionChanged()));
    m_eventLink->Connect(m_scalarOpacityFunction, vtkCommand::ModifiedEvent,
                         this, SLOT(onScalarOpacityFunctionChanged()));

    onColorFunctionChanged();
    resetAutoContrastState();
    emit colorMapUpdated();
  }

  updateColorMapDialogs();
}

void HistogramWidget::setLUTProxy(vtkSMProxy* proxy)
{
  if (proxy && m_LUTProxy != proxy) {
    m_LUTProxy = proxy;
    auto lut =
      vtkDiscretizableColorTransferFunction::SafeDownCast(
        proxy->GetClientSideObject());
    setLUT(lut);

    auto view = ActiveObjects::instance().activeView();

    // Update widget to reflect scalar bar visibility.
    if (m_LUTProxy) {
      auto sbProxy = getScalarBarRepresentation(view);
      if (sbProxy) {
        bool visible =
          vtkSMPropertyHelper(sbProxy, "Visibility").GetAsInt() == 1;
        m_colorLegendToolButton->setChecked(visible);
      }
    }
  }
}

void HistogramWidget::setVolumeData(pipeline::VolumeDataPtr volumeData)
{
  m_volumeData = std::move(volumeData);
}

void HistogramWidget::updateLUTProxy()
{
  // Update the LUT proxy from the LUT object
  auto* lutProxy = m_LUTProxy.Get();
  auto* lut = m_LUT.Get();

  if (!lutProxy || !lut) {
    return;
  }

  auto numNodes = lut->GetSize();
  auto* dataArray = lut->GetDataPointer();
  int nodeStride = 4;

  auto colorSpace = lut->GetColorSpace();

  auto* controlPointsProperty = lutProxy->GetProperty("RGBPoints");
  vtkSMPropertyHelper(controlPointsProperty)
    .Set(dataArray, numNodes * nodeStride);

  auto* colorSpaceProperty = lutProxy->GetProperty("ColorSpace");
  vtkSMPropertyHelper(colorSpaceProperty).Set(colorSpace);
}

void HistogramWidget::updateColorMapDialogs()
{
  auto* lut = m_LUT.Get();

  if (m_colorMapSettingsWidget) {
    m_colorMapSettingsWidget->setLut(lut);
    m_colorMapSettingsWidget->updateGui();
  }

  if (m_brightnessContrastWidget) {
    m_brightnessContrastWidget->setVolumeData(m_volumeData);
    m_brightnessContrastWidget->setLut(lut);
    m_brightnessContrastWidget->updateGui();
  }
}

void HistogramWidget::setInputData(vtkTable* table, const char* x_,
                                   const char* y_)
{
  m_inputData = table;
  m_histogramColorOpacityEditor->SetHistogramInputData(table, x_, y_);
  m_histogramColorOpacityEditor->SetOpacityFunction(m_scalarOpacityFunction);
  if (m_LUT && table) {
    m_histogramColorOpacityEditor->SetScalarVisibility(true);
    m_histogramColorOpacityEditor->SetColorTransferFunction(m_LUT);
    m_histogramColorOpacityEditor->SelectColorArray("image_extents");
  }
  m_histogramView->Render();
}

vtkSMProxy* HistogramWidget::getScalarBarRepresentation(vtkSMProxy* view)
{
  if (!view) {
    return nullptr;
  }

  auto tferProxy = vtkSMTransferFunctionProxy::SafeDownCast(m_LUTProxy);
  if (!tferProxy) {
    return nullptr;
  }

  auto sbProxy = tferProxy->FindScalarBarRepresentation(view);
  if (!sbProxy) {
    // No scalar bar representation exists yet, create it and initialize it
    // with some default settings.
    vtkNew<vtkSMTransferFunctionManager> tferManager;
    sbProxy = tferManager->GetScalarBarRepresentation(m_LUTProxy, view);
    vtkSMPropertyHelper(sbProxy, "Visibility").Set(0);
    vtkSMPropertyHelper(sbProxy, "Enabled").Set(0);
    vtkSMPropertyHelper(sbProxy, "Title").Set("");
    vtkSMPropertyHelper(sbProxy, "ComponentTitle").Set("");
    vtkSMPropertyHelper(sbProxy, "RangeLabelFormat").Set("{:g}");
    sbProxy->UpdateVTKObjects();
  }

  return sbProxy;
}

void HistogramWidget::onColorFunctionChanged()
{
  if (m_updatingColorFunction) {
    // Avoid infinite recursion
    return;
  }
  m_updatingColorFunction = true;

  updateLUTProxy();
  if (m_LUT) {
    m_LUT->Build();
    renderViews();
    emit colorMapUpdated();
  }

  m_updatingColorFunction = false;
}

void HistogramWidget::onScalarOpacityFunctionChanged()
{
  // Update rendered views of the data.
  ActiveObjects::instance().renderAllViews();

  // Update the histogram
  m_histogramView->GetRenderWindow()->Render();

  // Update the scalar opacity function proxy as it does not update its
  // internal state when the VTK object changes.
  if (!m_LUTProxy) {
    return;
  }

  auto opacityMapProxy =
    vtkSMPropertyHelper(m_LUTProxy, "ScalarOpacityFunction", true).GetAsProxy();
  if (!opacityMapProxy) {
    return;
  }

  vtkSMPropertyHelper pointsHelper(opacityMapProxy, "Points");
  auto opacityMapObject = opacityMapProxy->GetClientSideObject();
  auto pwf = vtkPiecewiseFunction::SafeDownCast(opacityMapObject);
  if (pwf) {
    pointsHelper.SetNumberOfElements(4 * pwf->GetSize());
    for (int i = 0; i < pwf->GetSize(); ++i) {
      double value[4];
      pwf->GetNodeValue(i, value);
      pointsHelper.Set(4 * i + 0, value[0]);
      pointsHelper.Set(4 * i + 1, value[1]);
      pointsHelper.Set(4 * i + 2, value[2]);
      pointsHelper.Set(4 * i + 3, value[3]);
    }
  }

  emit opacityChanged();
}

void HistogramWidget::onCurrentPointEditEvent()
{
  double rgb[3];
  if (m_histogramColorOpacityEditor->GetCurrentControlPointColor(rgb)) {
    QColor color =
      QColorDialog::getColor(QColor::fromRgbF(rgb[0], rgb[1], rgb[2]), this,
                             "Select Color for Control Point");
    if (color.isValid()) {
      rgb[0] = color.redF();
      rgb[1] = color.greenF();
      rgb[2] = color.blueF();
      m_histogramColorOpacityEditor->SetCurrentControlPointColor(rgb);
    }
  }
  ActiveObjects::instance().renderAllViews();
}

void HistogramWidget::histogramClicked(vtkObject*)
{
  auto& ao = ActiveObjects::instance();
  auto* view = ao.activeView();
  if (!view) {
    return;
  }

  auto isoValue = m_histogramColorOpacityEditor->GetContourValue();

  auto* pip = ao.pipeline();
  if (!pip) {
    return;
  }

  pipeline::ContourSink* contour = nullptr;

  // Priority 1: the active node is already a ContourSink.
  contour = qobject_cast<pipeline::ContourSink*>(ao.activeNode());

  // Priority 2: a ContourSink linked to the current tip output port.
  if (!contour) {
    auto* tipPort = ao.activeTipOutputPort();
    if (tipPort) {
      for (auto* link : tipPort->links()) {
        auto* sink =
          qobject_cast<pipeline::ContourSink*>(link->to()->node());
        if (sink) {
          contour = sink;
          break;
        }
      }
    }
  }

  // Priority 3: any ContourSink in the pipeline.
  if (!contour) {
    for (auto* node : pip->nodes()) {
      auto* sink = qobject_cast<pipeline::ContourSink*>(node);
      if (sink) {
        contour = sink;
        break;
      }
    }
  }

  // No existing ContourSink — ask the user to create one.
  if (!contour) {
    if (!createContourDialog(isoValue)) {
      return;
    }

    auto* tipPort = ao.activeTipOutputPort();
    if (!tipPort) {
      return;
    }

    auto* newContour = new pipeline::ContourSink();
    newContour->setLabel("Contour");
    newContour->initialize(view);
    pip->addNode(newContour);

    auto* input = newContour->inputPorts().value(0);
    if (input &&
        pipeline::isPortTypeCompatible(tipPort->type(),
                                       input->acceptedTypes())) {
      pip->createLink(tipPort, input);
    }

    contour = newContour;
  }

  contour->setIsoValue(isoValue);
  ao.setActiveNode(contour);
  pip->execute();

  tomviz::convert<pqView*>(view)->render();
}

bool HistogramWidget::createContourDialog(double& isoValue)
{
  QSettings* settings = pqApplicationCore::instance()->settings();
  bool autoAccept =
    settings->value("ContourSettings.AutoAccept", false).toBool();
  if (autoAccept) {
    return true;
  }

  // Obtain the scalar range from the tip output port's VolumeData.
  double range[2] = { 0.0, 1.0 };
  auto* tipPort = ActiveObjects::instance().activeTipOutputPort();
  if (tipPort && tipPort->hasData()) {
    auto vol = tipPort->data().value<pipeline::VolumeDataPtr>();
    if (vol && vol->isValid()) {
      auto r = vol->scalarRange();
      range[0] = r[0];
      range[1] = r[1];
    }
  }

  QDialog dialog;
  dialog.setFixedWidth(300);
  dialog.setMaximumHeight(50);
  dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  QVBoxLayout vLayout;
  dialog.setLayout(&vLayout);
  dialog.setWindowTitle(tr("New Iso Contour"));

  QFormLayout formLayout;
  vLayout.addLayout(&formLayout);

  DoubleSliderWidget w(true);
  w.setMinimum(range[0]);
  w.setMaximum(range[1]);

  // We want to round this to two decimal places
  isoValue = QString::number(isoValue, 'f', 2).toDouble();
  w.setValue(isoValue);

  w.setLineEditWidth(50);

  formLayout.addRow("Iso value", &w);

  QCheckBox dontAskAgain("Don't ask again");
  formLayout.addRow(&dontAskAgain);

  QDialogButtonBox buttons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  vLayout.addWidget(&buttons);

  connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  auto r = dialog.exec();

  if (r == QDialog::Accepted) {
    if (dontAskAgain.isChecked()) {
      settings->setValue("ContourSettings.AutoAccept", true);
    }
    isoValue = w.value();
    return true;
  } else {
    return false;
  }
}

void HistogramWidget::onResetRangeClicked()
{
  resetRange();
}

void HistogramWidget::resetRange()
{
  if (!m_volumeData || !m_volumeData->isValid()) {
    return;
  }

  auto sr = m_volumeData->scalarRange();
  double range[2] = { sr[0], sr[1] };
  resetRange(range);
}

void HistogramWidget::resetRange(double range[2])
{
  resetAutoContrastState();
  rescaleTransferFunction(m_LUTProxy, range[0], range[1]);
  renderViews();
}

void HistogramWidget::onCustomRangeClicked()
{
  if (!m_volumeData || !m_volumeData->isValid()) {
    return;
  }

  auto sr = m_volumeData->scalarRange();
  double maxRange[2] = { sr[0], sr[1] };

  // Get the type of the active scalar
  auto* array = m_volumeData->scalars();
  if (!array) {
    return;
  }
  auto dataType = array->GetDataType();
  int precision = 0;
  if (dataType == VTK_FLOAT || dataType == VTK_DOUBLE) {
    precision = 6;
  }

  // Get the current range
  vtkVector2d currentRange;
  vtkDiscretizableColorTransferFunction* discFunc =
    vtkDiscretizableColorTransferFunction::SafeDownCast(
      m_LUTProxy->GetClientSideObject());
  if (!discFunc) {
    return;
  }
  discFunc->GetRange(currentRange.GetData());

  QDialog dialog;
  QVBoxLayout vLayout;
  QHBoxLayout hLayout;
  vLayout.addLayout(&hLayout);

  // Fix the size of this window
  vLayout.setSizeConstraint(QLayout::SetFixedSize);
  hLayout.setSizeConstraint(QLayout::SetFixedSize);

  dialog.setLayout(&vLayout);
  dialog.setWindowTitle(tr("Specify Data Range"));

  QDoubleSpinBox bottom;
  bottom.setRange(maxRange[0], maxRange[1]);
  bottom.setValue(currentRange[0]);
  bottom.setDecimals(precision);
  bottom.setFixedSize(bottom.sizeHint());
  bottom.setToolTip("Min: " + QString::number(maxRange[0]));
  hLayout.addWidget(&bottom);

  QLabel dash("-");
  dash.setAlignment(Qt::AlignHCenter);
  dash.setAlignment(Qt::AlignVCenter);
  hLayout.addWidget(&dash);

  QDoubleSpinBox top;
  top.setRange(maxRange[0], maxRange[1]);
  top.setValue(currentRange[1]);
  top.setDecimals(precision);
  top.setFixedSize(top.sizeHint());
  top.setToolTip("Max: " + QString::number(maxRange[1]));
  hLayout.addWidget(&top);

  // Make sure the bottom isn't higher than the top, and the
  // top isn't higher than the bottom.
  connect(&bottom,
          static_cast<void (QDoubleSpinBox::*)(double)>(
            &QDoubleSpinBox::valueChanged),
          &top, &QDoubleSpinBox::setMinimum);
  connect(&top,
          static_cast<void (QDoubleSpinBox::*)(double)>(
            &QDoubleSpinBox::valueChanged),
          &bottom, &QDoubleSpinBox::setMaximum);

  QDialogButtonBox buttonBox;
  buttonBox.addButton(QDialogButtonBox::Ok);
  buttonBox.addButton(QDialogButtonBox::Cancel);
  connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  vLayout.addWidget(&buttonBox);

  if (dialog.exec() == QDialog::Accepted) {
    resetAutoContrastState();
    rescaleTransferFunction(m_LUTProxy, bottom.value(), top.value());
    renderViews();
  }

  // vLayout should not call 'delete' on hLayout...
  hLayout.setParent(nullptr);
}

void HistogramWidget::onInvertClicked()
{
  removePlaceholderNodes();
  vtkSMTransferFunctionProxy::InvertTransferFunction(m_LUTProxy);
  addPlaceholderNodes();
  resetAutoContrastState();
  renderViews();
  emit colorMapUpdated();
}

void HistogramWidget::onColorMapSettingsClicked()
{
  if (m_colorMapSettingsDialog) {
    // It's already visible
    return;
  }

  m_colorMapSettingsDialog = new QDialog(this);
  auto& dialog = *m_colorMapSettingsDialog;
  dialog.setLayout(new QVBoxLayout);
  dialog.setWindowTitle("Color map settings");

  m_colorMapSettingsWidget = new ColorMapSettingsWidget(m_LUT, this);
  dialog.layout()->addWidget(m_colorMapSettingsWidget);

  dialog.show();

  // Delete the dialog when it is closed
  connect(&dialog, &QDialog::finished, &dialog, &QDialog::deleteLater);
}

void HistogramWidget::showPresetDialog(const QJsonObject& newPreset)
{
  if (m_presetDialog == nullptr) {
    m_presetDialog = new PresetDialog(this);
    QObject::connect(m_presetDialog, &PresetDialog::applyPreset, this,
                     &HistogramWidget::applyCurrentPreset);
    QObject::connect(m_presetDialog,
                     &PresetDialog::createSegmentationColormapRequested, this,
                     &HistogramWidget::onCreateSegmentationColormapClicked);
  }

  if (!newPreset.isEmpty()) {
    m_presetDialog->addNewPreset(newPreset);
  }

  m_presetDialog->show();
}

void HistogramWidget::onSaveToPresetClicked()
{
  QDialog dialog;
  QVBoxLayout vLayout;
  QHBoxLayout hLayout;

  vLayout.addLayout(&hLayout);
  dialog.setLayout(&vLayout);
  dialog.setWindowTitle(tr("Create Preset"));

  QLineEdit name;
  name.setPlaceholderText("Enter name of new preset");
  hLayout.addWidget(&name);

  QDialogButtonBox buttonBox;
  buttonBox.addButton(QDialogButtonBox::Ok);
  buttonBox.addButton(QDialogButtonBox::Cancel);
  connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  vLayout.addWidget(&buttonBox);

  if (dialog.exec() == QDialog::Accepted) {
    auto newName = name.text();
    vtkSMProxy* lut = m_LUTProxy;
    auto presetInfo = tomviz::serialize(lut);
    auto presetColors = presetInfo["colors"];
    auto colorSpace = presetInfo["colorSpace"];
    QJsonObject newPreset{ { "name", newName },
                           { "colorSpace", colorSpace },
                           { "colors", presetColors },
			   { "default", QJsonValue(false) }
    };
    showPresetDialog(newPreset);
  }
}

void HistogramWidget::onPresetClicked()
{
  showPresetDialog(QJsonObject());
}

void HistogramWidget::resetAutoContrastState()
{
  m_currentAutoContrastThreshold = m_defaultAutoContrastThreshold;
}

void HistogramWidget::onBrightnessAndContrastClicked()
{
  if (m_brightnessContrastDialog) {
    // It's already visible
    return;
  }

  m_brightnessContrastDialog = new QDialog(this);
  auto& dialog = *m_brightnessContrastDialog;
  dialog.setLayout(new QVBoxLayout);
  dialog.setWindowTitle("Brightness and Contrast");

  m_brightnessContrastWidget =
    new BrightnessContrastWidget(m_volumeData, m_LUT, this);
  dialog.layout()->addWidget(m_brightnessContrastWidget);

  auto* widget = m_brightnessContrastWidget.data();
  connect(widget, &BrightnessContrastWidget::autoPressed, this,
          QOverload<>::of(&HistogramWidget::autoAdjustContrast));
  connect(widget, &BrightnessContrastWidget::resetPressed, this,
          QOverload<>::of(&HistogramWidget::resetRange));

  dialog.show();

  // Delete the dialog when it is closed
  connect(&dialog, &QDialog::finished, &dialog, &QDialog::deleteLater);
}

void HistogramWidget::autoAdjustContrast()
{
  auto* table = m_inputData.Get();

  if (!table || !m_volumeData || !m_volumeData->isValid() || !m_LUT) {
    return;
  }

  auto* imageData = m_volumeData->imageData();
  auto* histogram =
    vtkDataArray::SafeDownCast(table->GetColumnByName("image_pops"));
  auto* extents =
    vtkDataArray::SafeDownCast(table->GetColumnByName("image_extents"));

  if (!imageData || !histogram || !extents ||
      extents->GetNumberOfTuples() < 2) {
    return;
  }

  autoAdjustContrast(histogram, extents, imageData);
}

void HistogramWidget::autoAdjustContrast(vtkDataArray* histogram,
                                         vtkDataArray* extents,
                                         vtkImageData* imageData)
{
  // Gather some information
  auto sr = m_volumeData->scalarRange();
  double range[2] = { sr[0], sr[1] };

  int dims[3];
  imageData->GetDimensions(dims);

  auto voxelCount = static_cast<size_t>(dims[0]) * dims[1] * dims[2];
  auto numBins = histogram->GetNumberOfTuples();
  auto histMin = extents->GetTuple1(0);
  auto binSize = extents->GetTuple1(1) - histMin;
  auto& autoThreshold = m_currentAutoContrastThreshold;

  // Perform the operation as ImageJ does it
  auto limit = voxelCount / 10;
  if (autoThreshold < 10) {
    autoThreshold = m_defaultAutoContrastThreshold;
  } else {
    autoThreshold /= 2;
  }
  auto threshold = voxelCount / autoThreshold;

  int i;
  for (i = 0; i < numBins; ++i) {
    double count = histogram->GetTuple1(i);
    count = count > limit ? 0 : count;
    if (count > threshold) {
      break;
    }
  }
  int hmin = i;

  for (i = 255; i >= 0; --i) {
    double count = histogram->GetTuple1(i);
    count = count > limit ? 0 : count;
    if (count > threshold) {
      break;
    }
  }
  int hmax = i;

  if (hmax < hmin) {
    resetRange(range);
    return;
  }

  double min = histMin + hmin * binSize;
  double max = histMin + hmax * binSize;
  if (min == max) {
    min = range[0];
    max = range[1];
  }
  rescaleTransferFunction(m_LUTProxy, min, max);
}

void HistogramWidget::onCreateSegmentationColormapClicked()
{
  if (!m_volumeData || !m_volumeData->isValid()) {
    return;
  }

  auto* scalars = m_volumeData->scalars();
  if (!scalars) {
    return;
  }

  auto dataType = scalars->GetDataType();
  if (dataType == VTK_FLOAT || dataType == VTK_DOUBLE) {
    QMessageBox::warning(
      this, "Integer Data Required",
      "Segmentation colormaps require integer-valued data. "
      "The current dataset has floating-point values.");
    return;
  }

  // Collect unique values from the data
  std::set<double> uniqueValues;
  auto numTuples = scalars->GetNumberOfTuples();
  for (vtkIdType i = 0; i < numTuples; ++i) {
    uniqueValues.insert(scalars->GetTuple1(i));
  }

  if (uniqueValues.empty()) {
    return;
  }

  static const int maxSegments = 256;
  if (static_cast<int>(uniqueValues.size()) > maxSegments) {
    QMessageBox::warning(
      this, "Too Many Unique Values",
      QString("The data contains %1 unique values. Segmentation colormaps "
              "work best with discrete labeled data (max %2 segments).")
        .arg(uniqueValues.size())
        .arg(maxSegments));
    return;
  }

  // Generate distinct colors using golden angle hue spacing in HSV
  static const double goldenAngle = 137.508;
  QJsonArray colors;
  int idx = 0;
  for (double val : uniqueValues) {
    double hue = std::fmod(idx * goldenAngle, 360.0) / 360.0;
    double sat = 0.65 + 0.35 * ((idx % 3) / 2.0);
    double brightness = 0.75 + 0.25 * ((idx + 1) % 2);

    // HSV to RGB conversion
    double c = brightness * sat;
    double x = c * (1.0 - std::abs(std::fmod(hue * 6.0, 2.0) - 1.0));
    double m = brightness - c;

    double r, g, b;
    double h6 = hue * 6.0;
    if (h6 < 1.0) {
      r = c; g = x; b = 0;
    } else if (h6 < 2.0) {
      r = x; g = c; b = 0;
    } else if (h6 < 3.0) {
      r = 0; g = c; b = x;
    } else if (h6 < 4.0) {
      r = 0; g = x; b = c;
    } else if (h6 < 5.0) {
      r = x; g = 0; b = c;
    } else {
      r = c; g = 0; b = x;
    }

    colors.append(val);
    colors.append(r + m);
    colors.append(g + m);
    colors.append(b + m);
    ++idx;
  }

  QJsonObject newPreset{ { "name", "Segmentation" },
                         { "colorSpace", "Step" },
                         { "colors", colors } };
  m_presetDialog->addNewPreset(newPreset);
  applyCurrentPreset();
}

void HistogramWidget::applyCurrentPreset()
{
  vtkSMProxy* lut = m_LUTProxy;

  if (!lut) {
    return;
  }

  auto current = m_presetDialog->presetName();
  ColorMap::instance().applyPreset(current, lut);

  renderViews();
  resetAutoContrastState();
  emit colorMapUpdated();

  updateColorMapDialogs();
}

void HistogramWidget::updateUI()
{
  auto view = ActiveObjects::instance().activeView();

  // Update widget to reflect scalar bar visibility.
  if (m_LUTProxy) {
    auto sbProxy = getScalarBarRepresentation(view);
    if (view && sbProxy) {
      QSignalBlocker blocker1(m_colorLegendToolButton);
      QSignalBlocker blocker2(m_colorMapSettingsButton);
      QSignalBlocker blocker3(m_savePresetButton);
      QSignalBlocker blocker4(m_brightnessAndContrastButton);
      m_colorLegendToolButton->setEnabled(true);
      m_colorMapSettingsButton->setEnabled(true);
      m_colorLegendToolButton->setChecked(
        vtkSMPropertyHelper(sbProxy, "Visibility").GetAsInt() == 1);
      m_savePresetButton->setEnabled(true);
      m_brightnessAndContrastButton->setEnabled(true);
    }
  }

  auto* activeNode = ActiveObjects::instance().activeNode();
  if (!activeNode) {
    QSignalBlocker blocker1(m_colorLegendToolButton);
    QSignalBlocker blocker2(m_colorMapSettingsButton);
    QSignalBlocker blocker3(m_savePresetButton);
    QSignalBlocker blocker4(m_brightnessAndContrastButton);
    m_colorLegendToolButton->setEnabled(false);
    m_colorMapSettingsButton->setEnabled(false);
    m_savePresetButton->setEnabled(false);
    m_brightnessAndContrastButton->setEnabled(false);
  }
}

void HistogramWidget::renderViews()
{
  pqView* view =
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view) {
    view->render();
  }
}

void HistogramWidget::rescaleTransferFunction(vtkSMProxy* lutProxy, double min,
                                              double max)
{
  auto opacityMap =
    vtkSMPropertyHelper(m_LUTProxy, "ScalarOpacityFunction").GetAsProxy();

  removePlaceholderNodes();
  vtkSMTransferFunctionProxy::RescaleTransferFunction(lutProxy, min, max);
  vtkSMTransferFunctionProxy::RescaleTransferFunction(opacityMap, min, max);
  addPlaceholderNodes();

  emit colorMapUpdated();
}

void HistogramWidget::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  this->renderViews();
}

void HistogramWidget::addPlaceholderNodes()
{
  if (!m_volumeData || !m_volumeData->isValid()) {
    return;
  }

  auto sr = m_volumeData->scalarRange();
  double range[2] = { sr[0], sr[1] };

  auto* lut = m_LUT.Get();
  auto* opacity = m_scalarOpacityFunction.Get();

  if (lut) {
    tomviz::addPlaceholderNodes(lut, range);
  }

  if (opacity) {
    tomviz::addPlaceholderNodes(opacity, range);
  }
}

void HistogramWidget::removePlaceholderNodes()
{
  auto* lut = m_LUT.Get();
  auto* opacity = m_scalarOpacityFunction.Get();

  if (lut) {
    tomviz::removePlaceholderNodes(lut);
  }

  if (opacity) {
    tomviz::removePlaceholderNodes(opacity);
  }
}

} // namespace tomviz
