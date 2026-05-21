/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineOutlineSink_h
#define tomvizPipelineOutlineSink_h

#include "LegacyModuleSink.h"

#include <vtkNew.h>

class vtkActor;
class vtkGridAxesActor3D;
class vtkOutlineFilter;
class vtkPolyDataMapper;
class vtkProperty;

namespace tomviz {
namespace pipeline {

/// Bounding-box outline visualization sink with optional grid axes.
/// Matches the old ModuleOutline: outline box + configurable grid axes actor
/// with axis labels/titles.
class OutlineSink : public LegacyModuleSink
{
  Q_OBJECT

public:
  OutlineSink(QObject* parent = nullptr);
  ~OutlineSink() override;

  QIcon icon() const override;

  void setVisibility(bool visible) override;

  bool initialize(vtkSMViewProxy* view) override;
  bool finalize() override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  void color(double rgb[3]) const;
  void setColor(double r, double g, double b);

  QWidget* createSinkPropertiesWidget(QWidget* parent) override;

  /// Grid axes visibility.
  bool showGridAxes() const;
  void setShowGridAxes(bool show);

  /// Custom axis titles.
  QString xTitle() const;
  void setXTitle(const QString& title);
  QString yTitle() const;
  void setYTitle(const QString& title);
  QString zTitle() const;
  void setZTitle(const QString& title);

  /// Grid line generation (separate from axes visibility).
  bool generateGrid() const;
  void setGenerateGrid(bool gen);

  /// Whether to use custom axis titles instead of auto-generated ones.
  bool useCustomAxesTitles() const;
  void setUseCustomAxesTitles(bool use);

  void onMetadataChanged() override;

  void clearVisualization() override;

protected:
  bool consume(const QMap<QString, PortData>& inputs) override;

private:
  void updateGridAxesColor(double r, double g, double b);
  void updateGridAxesTitles();

  vtkNew<vtkOutlineFilter> m_outlineFilter;
  vtkNew<vtkPolyDataMapper> m_mapper;
  vtkNew<vtkActor> m_actor;
  vtkNew<vtkProperty> m_property;
  vtkNew<vtkGridAxesActor3D> m_gridAxes;
  bool m_showGridAxes = false;
  bool m_generateGrid = false;
  bool m_useCustomAxesTitles = false;
  QString m_xTitle = "X";
  QString m_yTitle = "Y";
  QString m_zTitle = "Z";
};

} // namespace pipeline
} // namespace tomviz

#endif
