/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSegmentSinkWidget_h
#define tomvizSegmentSinkWidget_h

#include <QScopedPointer>
#include <QWidget>

/**
 * \brief UI layer of SegmentSink.
 *
 * Exposes the editable ITK segmentation script, contour value,
 * representation, opacity and specular controls.
 * Signals are forwarded to SegmentSink.
 */

namespace Ui {
class SegmentSinkWidget;
} // namespace Ui

namespace tomviz {

class SegmentSinkWidget : public QWidget
{
  Q_OBJECT

public:
  SegmentSinkWidget(QWidget* parent_ = nullptr);
  ~SegmentSinkWidget() override;

  //@{
  /**
   * UI update methods.
   */
  void setScript(const QString& script);
  void setContourValue(double value);
  void setContourRange(double range[2]);
  void setRepresentation(const QString& representation);
  void setOpacity(double value);
  void setSpecular(double value);
  //@}

signals:
  //@{
  /**
   * Forwarded signals.
   */
  void scriptApplied(const QString& script);
  void contourValueChanged(double value);
  void representationChanged(const QString& representation);
  void opacityChanged(double value);
  void specularChanged(double value);
  //@}

private:
  void onRepresentationIndexChanged(int i);

  SegmentSinkWidget(const SegmentSinkWidget&) = delete;
  void operator=(const SegmentSinkWidget&) = delete;

  QScopedPointer<Ui::SegmentSinkWidget> m_ui;
};
} // namespace tomviz
#endif
