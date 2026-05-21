/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePlotSink_h
#define tomvizPipelinePlotSink_h

#include "LegacyModuleSink.h"

#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

#include <QList>

class vtkChartXY;
class vtkPlot;
class vtkPVContextView;
class vtkTable;

namespace tomviz {
namespace pipeline {

/// Line/chart plot visualization sink for table data.
/// Uses a vtkPVContextView (chart view) obtained from the vtkSMViewProxy
/// passed to initialize().
class PlotSink : public LegacyModuleSink
{
  Q_OBJECT

public:
  PlotSink(QObject* parent = nullptr);
  ~PlotSink() override;

  QIcon icon() const override;

  void setVisibility(bool visible) override;

  bool initialize(vtkSMViewProxy* view) override;
  bool finalize() override;

  void clearVisualization() override;

  QWidget* createSinkPropertiesWidget(QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  /// Axis labels.
  QString xLabel() const;
  void setXLabel(const QString& label);
  QString yLabel() const;
  void setYLabel(const QString& label);

  /// Log scale toggles.
  bool xLogScale() const;
  void setXLogScale(bool log);
  bool yLogScale() const;
  void setYLogScale(bool log);

protected:
  bool consume(const QMap<QString, PortData>& inputs) override;

private:
  void addAllPlots();
  void removeAllPlots();

  vtkWeakPointer<vtkPVContextView> m_contextView;
  vtkWeakPointer<vtkChartXY> m_chart;
  vtkSmartPointer<vtkTable> m_table;
  QList<vtkSmartPointer<vtkPlot>> m_plots;

  QString m_xLabel;
  QString m_yLabel;
  bool m_xLogScale = false;
  bool m_yLogScale = false;
};

} // namespace pipeline
} // namespace tomviz

#endif
