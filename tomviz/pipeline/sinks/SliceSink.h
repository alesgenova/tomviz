/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineSliceSink_h
#define tomvizPipelineSliceSink_h

#include "LegacyModuleSink.h"

#include <vtkNew.h>
#include <vtkSmartPointer.h>

#include <array>

class vtkActiveScalarsProducer;
class vtkNonOrthoImagePlaneWidget;
class vtkScalarsToColors;

namespace tomviz {
namespace pipeline {

/// Slice visualization sink using vtkNonOrthoImagePlaneWidget.
/// Supports orthogonal (XY, YZ, XZ) and custom (arbitrary plane) slicing
/// with interactive widget, thick slicing, texture interpolation, and more.
class SliceSink : public LegacyModuleSink
{
  Q_OBJECT

public:
  enum Direction { XY = 0, YZ = 1, XZ = 2, Custom = 3 };
  Q_ENUM(Direction)

  enum ThickSliceMode { Min = 0, Max = 1, Mean = 2, Sum = 3 };
  Q_ENUM(ThickSliceMode)

  SliceSink(QObject* parent = nullptr);
  ~SliceSink() override;

  QIcon icon() const override;

  void setVisibility(bool visible) override;
  bool isColorMapNeeded() const override;

  bool initialize(vtkSMViewProxy* view) override;
  bool finalize() override;

  void clearVisualization() override;

  QWidget* createSinkPropertiesWidget(QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  /// Direction (orthogonal axis or custom plane).
  Direction direction() const;
  void setDirection(Direction dir);

  /// Whether direction is orthogonal (not Custom).
  bool isOrtho() const;

  /// Slice index (valid only for orthogonal directions).
  int slice() const;
  void setSlice(int index);

  /// Maximum slice index for the current direction.
  int maxSlice() const;

  /// Opacity of the slice plane.
  double opacity() const;
  void setOpacity(double value);

  /// Thick slicing: number of slices to composite (1 = single slice).
  int sliceThickness() const;
  void setSliceThickness(int slices);

  /// Thick slicing aggregation mode (Min, Max, Mean, Sum).
  ThickSliceMode thickSliceMode() const;
  void setThickSliceMode(ThickSliceMode mode);

  /// Texture interpolation (linear vs. nearest-neighbor).
  bool textureInterpolate() const;
  void setTextureInterpolate(bool interpolate);

  /// Arrow visibility on the slice widget.
  bool showArrow() const;
  void setShowArrow(bool show);

  /// Whether to map scalars through a color map.
  bool mapScalars() const;
  void setMapScalars(bool map);

  /// Active scalar array index (-1 = use default active scalars).
  int activeScalars() const;
  void setActiveScalars(int index);

  /// Set the center (point on the plane) for Custom direction.
  void setPlaneCenter(double x, double y, double z);
  void planeCenter(double xyz[3]) const;

  /// Set the normal for Custom direction.
  void setPlaneNormal(double x, double y, double z);
  void planeNormal(double xyz[3]) const;

  void addClippingPlane(vtkPlane* plane) override;
  void removeClippingPlane(vtkPlane* plane) override;

  void onMetadataChanged() override;

signals:
  void sliceChanged(int slice);
  void directionChanged(Direction direction);
  void planeChanged();

protected:
  bool consume(const QMap<QString, PortData>& inputs) override;
  void updateColorMap() override;

private slots:
  void onPlaneChanged();

private:
  void setupWidget();
  void applyDirection();
  void applyActiveScalars();
  int directionAxis() const;

  vtkSmartPointer<vtkNonOrthoImagePlaneWidget> m_widget;
  vtkNew<vtkActiveScalarsProducer> m_producer;

  Direction m_direction = XY;
  int m_slice = -1;
  int m_sliceThickness = 1;
  ThickSliceMode m_thickSliceMode = Mean;
  bool m_interpolate = false;
  double m_opacity = 1.0;
  bool m_showArrow = true;
  bool m_mapScalars = true;
  int m_activeScalars = -1;

  // For custom direction
  double m_planeCenter[3] = { 0, 0, 0 };
  double m_planeNormal[3] = { 0, 0, 1 };
  bool m_planeCenterSet = false;

  bool m_dataReceived = false;
  int m_dims[3] = { 0, 0, 0 };
  double m_bounds[6] = { 0, 0, 0, 0, 0, 0 };
  std::array<double, 3> m_lastSpacing = { 0.0, 0.0, 0.0 };
  std::array<double, 3> m_lastOrigin = { 0.0, 0.0, 0.0 };
};

} // namespace pipeline
} // namespace tomviz

#endif
