/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineRulerSink_h
#define tomvizPipelineRulerSink_h

#include "LegacyModuleSink.h"

#include <QPointer>
#include <vtkNew.h>
#include <vtkSmartPointer.h>

class pqLinePropertyWidget;
class vtkActor;
class vtkBillboardTextActor3D;
class vtkImageData;
class vtkLineSource;
class vtkPolyDataMapper;
class vtkSMSourceProxy;

namespace tomviz {
namespace pipeline {

/// Ruler/measurement annotation visualization sink.
/// Displays a measurement line between two user-defined points with
/// a distance label and units.  Provides an interactive 3D line widget
/// (pqLinePropertyWidget) for adjusting endpoints.
/// Matches the old ModuleRuler feature set.
class RulerSink : public LegacyModuleSink
{
  Q_OBJECT

public:
  RulerSink(QObject* parent = nullptr);
  ~RulerSink() override;

  QIcon icon() const override;

  void setVisibility(bool visible) override;

  bool initialize(vtkSMViewProxy* view) override;
  bool finalize() override;

  void clearVisualization() override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  QWidget* createSinkPropertiesWidget(QWidget* parent) override;

  void onMetadataChanged() override;

signals:
  void newEndpointData(double val1, double val2);

protected:
  bool consume(const QMap<QString, PortData>& inputs) override;

private slots:
  void endPointsUpdated();

private:
  /// Build or update the SM proxy and VTK actors on the main thread.
  void setupOrUpdatePipeline();
  /// Sync the VTK line and distance label with the proxy's Point1/Point2.
  void updateRulerVisual();
  /// Update the distance label units string from the volume data.
  void updateUnits();

  // SM source proxy — needed for pqLinePropertyWidget binding.
  // We intentionally do NOT create an SM representation (via
  // controller->Show) because vtkRulerSourceRepresentation has a
  // crash bug in ProcessViewRequest during selection renders.
  vtkSmartPointer<vtkSMSourceProxy> m_rulerSource;

  // Raw VTK visual: persistent line + distance label.
  vtkNew<vtkLineSource> m_lineSource;
  vtkNew<vtkPolyDataMapper> m_mapper;
  vtkNew<vtkActor> m_lineActor;
  vtkNew<vtkBillboardTextActor3D> m_textActor;

  vtkSmartPointer<vtkImageData> m_pendingImage;
  QPointer<pqLinePropertyWidget> m_widget;

  bool m_showLine = true;
  QString m_units;

  // Deserialized points to apply when SM pipeline is created.
  double m_pendingPoint1[3] = { 0, 0, 0 };
  double m_pendingPoint2[3] = { 0, 0, 0 };
  bool m_hasPendingPoints = false;
};

} // namespace pipeline
} // namespace tomviz

#endif
