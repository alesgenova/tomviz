/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizInteractiveTransformWidget_h
#define tomvizInteractiveTransformWidget_h

#include <QObject>
#include <QPointer>

#include <vtkNew.h>

class pqView;
class vtkCustomBoxRepresentation;
class vtkBoxWidget2;
class vtkEventQtSlotConnect;
class vtkObject;

namespace tomviz {

/// A singleton 3D box widget for interactive translation, rotation, and
/// scaling. Only one consumer may use the widget at a time via acquire/release.
class InteractiveTransformWidget : public QObject
{
  Q_OBJECT

public:
  static InteractiveTransformWidget& instance();

  /// Acquire exclusive use of the widget. Returns false if already held by
  /// another user. Auto-releases if @a user is destroyed.
  bool acquire(QObject* user);

  /// Release the widget. No-op if @a user is not the current holder.
  void release(QObject* user);

  /// Returns the current holder, or nullptr.
  QObject* currentUser() const { return m_currentUser; }

  /// Set the view whose interactor the widget should attach to.
  void setView(pqView* view);

  /// Enable/disable individual interaction modes.
  void setTranslationEnabled(bool enabled);
  void setRotationEnabled(bool enabled);
  void setScalingEnabled(bool enabled);

  /// Place the widget using bounding box extents.
  void setBounds(const double bounds[6]);

  /// Set the current widget transform (position, orientation, scale).
  void setTransform(const double position[3], const double orientation[3],
                    const double scale[3]);

signals:
  /// Emitted during and at the end of user interaction.
  void transformChanged(const double position[3],
                        const double orientation[3],
                        const double scale[3]);

  /// Emitted when the widget is released (including auto-release on user
  /// destruction).
  void widgetReleased();

protected:
  InteractiveTransformWidget();
  ~InteractiveTransformWidget() override;

private slots:
  void onInteraction(vtkObject* caller);
  void onUserDestroyed();

private:
  void updateWidgetState();
  void disableWidget();
  void render();

  Q_DISABLE_COPY(InteractiveTransformWidget)

  vtkNew<vtkCustomBoxRepresentation> m_boxRep;
  vtkNew<vtkBoxWidget2> m_boxWidget;
  vtkNew<vtkEventQtSlotConnect> m_eventLink;
  QPointer<pqView> m_view;
  QObject* m_currentUser = nullptr;

  bool m_translationEnabled = false;
  bool m_rotationEnabled = false;
  bool m_scalingEnabled = false;
};

} // namespace tomviz

#endif
