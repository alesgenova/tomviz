/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineSegmentSink_h
#define tomvizPipelineSegmentSink_h

#include "LegacyModuleSink.h"

#include <QPointer>
#include <vtkNew.h>

#include <QString>

class vtkActor;
class vtkDataSetMapper;
class vtkFlyingEdges3D;
class vtkImageData;
class vtkProperty;

namespace tomviz {

class SegmentSinkWidget;

namespace pipeline {

/// Segmentation visualization sink.
/// Matches the old ModuleSegment: runs a user-editable Python/ITK
/// segmentation script on the input volume, then displays a contour
/// of the segmentation result.
class SegmentSink : public LegacyModuleSink
{
  Q_OBJECT

public:
  SegmentSink(QObject* parent = nullptr);
  ~SegmentSink() override;

  QIcon icon() const override;

  void setVisibility(bool visible) override;
  bool isColorMapNeeded() const override;

  bool initialize(vtkSMViewProxy* view) override;
  bool finalize() override;

  void clearVisualization() override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  /// User-editable ITK segmentation script.
  /// Should define run_itk_segmentation(itk_image, itk_image_type)
  /// which returns (output_image, output_image_type).
  QString script() const;
  void setScript(const QString& script);

  /// Contour value applied to the segmentation result (default 0.5).
  double contourValue() const;
  void setContourValue(double value);

  double opacity() const;
  void setOpacity(double value);

  double specular() const;
  void setSpecular(double value);

  /// Representation: VTK_SURFACE (2), VTK_WIREFRAME (1), VTK_POINTS (0).
  int representation() const;
  void setRepresentation(int rep);

  /// String-based representation: "Surface", "Wireframe", "Points".
  QString representationString() const;
  void setRepresentationString(const QString& rep);

  QWidget* createSinkPropertiesWidget(QWidget* parent) override;

  void addClippingPlane(vtkPlane* plane) override;
  void removeClippingPlane(vtkPlane* plane) override;

  void onMetadataChanged() override;

protected:
  bool consume(const QMap<QString, PortData>& inputs) override;
  void updateColorMap() override;
  void updatePanel();

private:
  bool runSegmentation(vtkImageData* input);

  vtkNew<vtkFlyingEdges3D> m_contour;
  vtkNew<vtkDataSetMapper> m_mapper;
  vtkNew<vtkActor> m_actor;
  vtkNew<vtkProperty> m_property;
  vtkNew<vtkImageData> m_segmentedData;
  QString m_script;
  double m_contourValue = 0.5;
  QPointer<SegmentSinkWidget> m_controllers;
};

} // namespace pipeline
} // namespace tomviz

#endif
