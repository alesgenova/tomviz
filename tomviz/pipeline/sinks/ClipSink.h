/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineClipSink_h
#define tomvizPipelineClipSink_h

#include "LegacyModuleSink.h"

#include <vtkNew.h>
#include <vtkSmartPointer.h>

#include <QSet>

#include <array>

class vtkImageData;
class vtkNonOrthoImagePlaneWidget;
class vtkPlane;

namespace tomviz {
namespace pipeline {

class Link;
class OutputPort;

/// Clipping-plane visualization sink using vtkNonOrthoImagePlaneWidget.
/// Matches the old ModuleClip: shows an interactive texture-mapped plane
/// and emits the clipping plane geometry for other modules to clip against.
class ClipSink : public LegacyModuleSink
{
  Q_OBJECT

public:
  enum Direction { XY = 0, YZ = 1, XZ = 2, Custom = 3 };

  ClipSink(QObject* parent = nullptr);
  ~ClipSink() override;

  QIcon icon() const override;

  void setVisibility(bool visible) override;

  bool initialize(vtkSMViewProxy* view) override;
  bool finalize() override;

  void clearVisualization() override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  /// Plane orientation preset.
  Direction direction() const;
  void setDirection(Direction dir);

  /// Slice index for orthogonal directions (0-based).
  int slice() const;
  void setSlice(int s);

  /// Plane opacity.
  double opacity() const;
  void setOpacity(double value);

  /// Show/hide the directional arrow on the widget.
  bool showArrow() const;
  void setShowArrow(bool show);

  /// Invert clip direction.
  bool invertPlane() const;
  void setInvertPlane(bool invert);

  /// Show/hide the plane texture (independent of arrow visibility).
  bool showPlane() const;
  void setShowPlane(bool show);

  /// Plane color.
  void planeColor(double rgb[3]) const;
  void setPlaneColor(double r, double g, double b);

  /// Read the current plane center and normal from the widget/plane.
  void planeCenter(double center[3]) const;
  void planeNormal(double normal[3]) const;

  /// Custom plane origin and normal.
  void setPlaneOrigin(double x, double y, double z);
  void setPlaneNormal(double nx, double ny, double nz);

  /// Max slice index for the current direction (-1 if Custom).
  int maxSlice() const;

  /// Direction axis: XY→2, YZ→0, XZ→1, Custom→-1.
  int directionAxis() const;

  /// True if direction is not Custom.
  bool isOrtho() const;

  /// Access the clipping plane for use by other sinks/modules.
  vtkPlane* clippingPlane() const;

  QWidget* createSinkPropertiesWidget(QWidget* parent) override;

  void onMetadataChanged() override;

signals:
  /// Emitted when the clip plane geometry is updated.
  void clipPlaneUpdated();
  /// Emitted when the slice index changes (from widget interaction).
  void sliceChanged(int slice);

protected:
  bool consume(const QMap<QString, PortData>& inputs) override;

private:
  void setupWidget();
  void applyDirection();

  // Sync m_clippingPlane from the widget, transforming data-coordinate
  // center/normal into world coordinates via the volume's display transform.
  void syncClippingPlane();

  // Called when the user drags the widget interactively
  void onWidgetInteraction();

  // Clipping plane propagation to sibling sinks
  void connectToSiblings();
  void disconnectFromSiblings();
  void onInputConnectionChanged();
  void onPipelineLinkCreated(Link* link);
  void onPipelineLinkRemoved(Link* link);

  vtkSmartPointer<vtkNonOrthoImagePlaneWidget> m_widget;
  vtkNew<vtkPlane> m_clippingPlane;
  unsigned long m_interactionTag = 0;
  Direction m_direction = XY;
  int m_slice = -1;
  double m_opacity = 0.5;
  bool m_showPlane = true;
  bool m_showArrow = true;
  bool m_invertPlane = false;
  double m_planeColor[3] = { 204.0 / 255, 204.0 / 255, 204.0 / 255 };
  int m_dims[3] = { 0, 0, 0 };
  double m_bounds[6] = { 0, 0, 0, 0, 0, 0 };

  std::array<double, 3> m_lastSpacing = { 0.0, 0.0, 0.0 };
  std::array<double, 3> m_lastOrigin = { 0.0, 0.0, 0.0 };
  QSet<LegacyModuleSink*> m_clippedSinks;
  OutputPort* m_upstreamPort = nullptr;
};

} // namespace pipeline
} // namespace tomviz

#endif
