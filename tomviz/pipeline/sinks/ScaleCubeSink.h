/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineScaleCubeSink_h
#define tomvizPipelineScaleCubeSink_h

#include "LegacyModuleSink.h"

#include <vtkNew.h>

class vtkHandleWidget;
class vtkMeasurementCubeHandleRepresentation3D;

namespace tomviz {
namespace pipeline {

/// Scale cube annotation visualization sink.
/// Displays an interactive cube of known size for spatial reference with
/// optional text annotation.  The user can drag the cube in the 3D view.
/// Matches the old ModuleScaleCube.
class ScaleCubeSink : public LegacyModuleSink
{
  Q_OBJECT

public:
  ScaleCubeSink(QObject* parent = nullptr);
  ~ScaleCubeSink() override;

  QIcon icon() const override;

  void setVisibility(bool visible) override;

  bool initialize(vtkSMViewProxy* view) override;
  bool finalize() override;

  void clearVisualization() override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  QWidget* createSinkPropertiesWidget(QWidget* parent) override;

  double sideLength() const;
  void setSideLength(double length);

  void position(double pos[3]) const;
  void setPosition(double x, double y, double z);

  bool adaptiveScaling() const;
  void setAdaptiveScaling(bool adaptive);

  void color(double rgb[3]) const;
  void setColor(double r, double g, double b);

  void textColor(double rgb[3]) const;
  void setTextColor(double r, double g, double b);

  bool showAnnotation() const;
  void setShowAnnotation(bool show);

  QString lengthUnit() const;
  void setLengthUnit(const QString& unit);

  void onMetadataChanged() override;

signals:
  void sideLengthChanged(double length);
  void positionChanged(double x, double y, double z);
  void lengthUnitChanged(const QString& unit);

protected:
  bool consume(const QMap<QString, PortData>& inputs) override;

private:
  void observeModified();
  vtkNew<vtkHandleWidget> m_handleWidget;
  vtkNew<vtkMeasurementCubeHandleRepresentation3D> m_cubeRep;
  unsigned long m_observedId = 0;
  bool m_annotationVisibility = true;
  QString m_lengthUnit;
  bool m_firstConsume = true;
};

} // namespace pipeline
} // namespace tomviz

#endif
