/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PlotSink.h"

#include <vtkAxis.h>
#include <vtkChartXY.h>
#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkFieldData.h>
#include <vtkPVContextView.h>
#include <vtkPlotLine.h>
#include <vtkSMViewProxy.h>
#include <vtkStringArray.h>
#include <vtkTable.h>
#include <vtkUnsignedCharArray.h>

#include <QCheckBox>
#include <QColor>
#include <QFormLayout>
#include <QLineEdit>
#include <QWidget>

namespace tomviz {
namespace pipeline {

PlotSink::PlotSink(QObject* parent) : LegacyModuleSink(parent)
{
  addInput("table", PortType::Table);
  setLabel("Plot");
}

PlotSink::~PlotSink()
{
  finalize();
}

QIcon PlotSink::icon() const
{
  return QIcon(QStringLiteral(":/pqWidgets/Icons/pqLineChart16.png"));
}

void PlotSink::setVisibility(bool visible)
{
  if (visible) {
    addAllPlots();
  } else {
    removeAllPlots();
  }
  LegacyModuleSink::setVisibility(visible);
}

bool PlotSink::initialize(vtkSMViewProxy* view)
{
  if (!LegacyModuleSink::initialize(view)) {
    return false;
  }

  m_contextView =
    vtkPVContextView::SafeDownCast(view->GetClientSideView());
  if (!m_contextView) {
    return false;
  }

  auto* contextView = m_contextView->GetContextView();
  m_chart = vtkChartXY::SafeDownCast(contextView->GetScene()->GetItem(0));
  if (!m_chart) {
    return false;
  }

  return true;
}

bool PlotSink::finalize()
{
  removeAllPlots();
  m_chart = nullptr;
  m_contextView = nullptr;
  return LegacyModuleSink::finalize();
}

void PlotSink::clearVisualization()
{
  removeAllPlots();
  m_table = nullptr;
  if (m_contextView) {
    m_contextView->Update();
  }
}

bool PlotSink::consume(const QMap<QString, PortData>& inputs)
{
  if (!validateInput(inputs, "table")) {
    return false;
  }

  auto tablePtr = inputs["table"].value<vtkSmartPointer<vtkTable>>();
  if (!tablePtr) {
    return false;
  }

  m_table = tablePtr;

  // Defer VTK chart manipulation to the main thread — consume() runs on
  // the pipeline worker thread, but vtkChartXY / vtkPVContextView objects
  // belong to the GUI thread.
  QMetaObject::invokeMethod(this, [this]() {
    if (m_chart) {
      addAllPlots();

      if (m_contextView) {
        m_contextView->Update();
      }
    }
    emit renderNeeded();
  }, Qt::QueuedConnection);

  return true;
}

void PlotSink::addAllPlots()
{
  removeAllPlots();

  if (!m_table || !m_chart) {
    return;
  }

  // Read axis labels from field data if present
  auto* fieldData = m_table->GetFieldData();
  auto* labelsArray = vtkStringArray::SafeDownCast(
    fieldData->GetAbstractArray("axes_labels"));
  if (labelsArray && labelsArray->GetNumberOfTuples() >= 2) {
    auto* xAxis = m_chart->GetAxis(vtkAxis::BOTTOM);
    auto* yAxis = m_chart->GetAxis(vtkAxis::LEFT);
    xAxis->SetTitle(labelsArray->GetValue(0));
    yAxis->SetTitle(labelsArray->GetValue(1));
    m_xLabel = QString::fromStdString(labelsArray->GetValue(0));
    m_yLabel = QString::fromStdString(labelsArray->GetValue(1));
  } else if (!m_xLabel.isEmpty() || !m_yLabel.isEmpty()) {
    m_chart->GetAxis(vtkAxis::BOTTOM)->SetTitle(m_xLabel.toStdString());
    m_chart->GetAxis(vtkAxis::LEFT)->SetTitle(m_yLabel.toStdString());
  }

  // Read log scale from field data if present
  auto* logScaleArray = vtkUnsignedCharArray::SafeDownCast(
    fieldData->GetAbstractArray("axes_log_scale"));
  if (logScaleArray && logScaleArray->GetNumberOfTuples() >= 2) {
    m_xLogScale = logScaleArray->GetValue(0) != 0;
    m_yLogScale = logScaleArray->GetValue(1) != 0;
    m_chart->GetAxis(vtkAxis::BOTTOM)->SetLogScale(m_xLogScale);
    m_chart->GetAxis(vtkAxis::LEFT)->SetLogScale(m_yLogScale);
  }

  // Offset colors so multiple PlotSinks sharing a chart get distinct colors
  int colorOffset = m_chart->GetNumberOfPlots();

  vtkIdType numCols = m_table->GetNumberOfColumns();
  for (vtkIdType col = 1; col < numCols; ++col) {
    auto line = vtkSmartPointer<vtkPlotLine>::New();
    int idx = colorOffset + col - 1;
    // Golden-angle hue spacing for maximum color separation
    int hue = (idx * 137) % 360;
    QColor color = QColor::fromHsv(hue, 200, 200);
    line->SetInputData(m_table, 0, col);
    line->SetColor(color.red(), color.green(), color.blue(), 255);
    line->SetWidth(3.0);
    m_chart->AddPlot(line);
    m_plots.append(line);
  }
}

void PlotSink::removeAllPlots()
{
  if (!m_chart) {
    return;
  }

  for (auto& plot : m_plots) {
    m_chart->RemovePlotInstance(plot);
  }
  m_plots.clear();
}

QString PlotSink::xLabel() const
{
  return m_xLabel;
}

void PlotSink::setXLabel(const QString& label)
{
  m_xLabel = label;
  if (m_chart) {
    m_chart->GetAxis(vtkAxis::BOTTOM)->SetTitle(label.toStdString());
    emit renderNeeded();
  }
}

QString PlotSink::yLabel() const
{
  return m_yLabel;
}

void PlotSink::setYLabel(const QString& label)
{
  m_yLabel = label;
  if (m_chart) {
    m_chart->GetAxis(vtkAxis::LEFT)->SetTitle(label.toStdString());
    emit renderNeeded();
  }
}

bool PlotSink::xLogScale() const
{
  return m_xLogScale;
}

void PlotSink::setXLogScale(bool log)
{
  m_xLogScale = log;
  if (m_chart) {
    m_chart->GetAxis(vtkAxis::BOTTOM)->SetLogScale(log);
    emit renderNeeded();
  }
}

bool PlotSink::yLogScale() const
{
  return m_yLogScale;
}

void PlotSink::setYLogScale(bool log)
{
  m_yLogScale = log;
  if (m_chart) {
    m_chart->GetAxis(vtkAxis::LEFT)->SetLogScale(log);
    emit renderNeeded();
  }
}

QWidget* PlotSink::createSinkPropertiesWidget(QWidget* parent)
{
  auto* widget = new QWidget(parent);
  auto* layout = new QFormLayout;

  QString xLabel, yLabel;
  bool xLog = m_xLogScale;
  bool yLog = m_yLogScale;

  if (m_chart) {
    xLabel = m_chart->GetAxis(vtkAxis::BOTTOM)->GetTitle().c_str();
    yLabel = m_chart->GetAxis(vtkAxis::LEFT)->GetTitle().c_str();
    xLog = m_chart->GetAxis(vtkAxis::BOTTOM)->GetLogScale();
    yLog = m_chart->GetAxis(vtkAxis::LEFT)->GetLogScale();
  }
  if (xLabel.isEmpty()) {
    xLabel = m_xLabel;
  }
  if (yLabel.isEmpty()) {
    yLabel = m_yLabel;
  }

  auto* xLabelEdit = new QLineEdit(xLabel);
  auto* yLabelEdit = new QLineEdit(yLabel);
  layout->addRow("X Label", xLabelEdit);
  layout->addRow("Y Label", yLabelEdit);

  auto* xLogCheck = new QCheckBox("X Log Scale");
  auto* yLogCheck = new QCheckBox("Y Log Scale");
  xLogCheck->setChecked(xLog);
  yLogCheck->setChecked(yLog);
  layout->addRow(xLogCheck);
  layout->addRow(yLogCheck);

  connect(xLabelEdit, &QLineEdit::textChanged, this, &PlotSink::setXLabel);
  connect(yLabelEdit, &QLineEdit::textChanged, this, &PlotSink::setYLabel);
  connect(xLogCheck, &QCheckBox::toggled, this, &PlotSink::setXLogScale);
  connect(yLogCheck, &QCheckBox::toggled, this, &PlotSink::setYLogScale);

  widget->setLayout(layout);
  return widget;
}

QJsonObject PlotSink::serialize() const
{
  auto json = LegacyModuleSink::serialize();
  json["xLabel"] = m_xLabel;
  json["yLabel"] = m_yLabel;
  json["xLogScale"] = m_xLogScale;
  json["yLogScale"] = m_yLogScale;
  return json;
}

bool PlotSink::deserialize(const QJsonObject& json)
{
  if (!LegacyModuleSink::deserialize(json)) {
    return false;
  }
  if (json.contains("xLabel")) {
    m_xLabel = json["xLabel"].toString();
  }
  if (json.contains("yLabel")) {
    m_yLabel = json["yLabel"].toString();
  }
  if (json.contains("xLogScale")) {
    setXLogScale(json["xLogScale"].toBool());
  }
  if (json.contains("yLogScale")) {
    setYLogScale(json["yLogScale"].toBool());
  }
  return true;
}

} // namespace pipeline
} // namespace tomviz
