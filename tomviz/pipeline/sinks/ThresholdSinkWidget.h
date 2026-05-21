/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizThresholdSinkWidget_h
#define tomvizThresholdSinkWidget_h

#include <QScopedPointer>
#include <QWidget>

/**
 * \brief UI layer of ThresholdSink.
 *
 * Signals are forwarded to ThresholdSink. This class is intended to contain
 * only logic related to UI actions.
 */

namespace Ui {
class ThresholdSinkWidget;
} // namespace Ui

namespace tomviz {

class ThresholdSinkWidget : public QWidget
{
  Q_OBJECT

public:
  ThresholdSinkWidget(QWidget* parent_ = nullptr);
  ~ThresholdSinkWidget() override;

  void setThresholdRange(double range[2]);
  void setThresholdByArrayOptions(const QStringList& scalars,
                                  int activeScalar);
  void setColorByArrayOptions(const QStringList& options);

  //@{
  /**
   * UI update methods. The actual model state is stored in ThresholdSink for
   * these parameters, so the UI needs to be updated if the state changes or
   * when constructing the UI.
   */
  void setColorMapData(bool state);
  void setMinimum(double value);
  void setMaximum(double value);
  void setRepresentation(const QString& representation);
  void setOpacity(double value);
  void setSpecular(double value);
  void setThresholdByArrayValue(int i);
  void setColorByArray(bool state);
  void setColorByArrayName(const QString& name);
  //@}

signals:
  //@{
  /**
   * Forwarded signals.
   */
  void colorMapDataToggled(bool state);
  void minimumChanged(double value);
  void maximumChanged(double value);
  void representationChanged(const QString& representation);
  void opacityChanged(double value);
  void specularChanged(double value);
  void thresholdByArrayValueChanged(int i);
  void colorByArrayToggled(bool state);
  void colorByArrayNameChanged(const QString& name);
  //@}

private:
  void onRepresentationIndexChanged(int i);
  void onThresholdByArrayIndexChanged(int i);
  void onColorByArrayIndexChanged(int i);

  ThresholdSinkWidget(const ThresholdSinkWidget&) = delete;
  void operator=(const ThresholdSinkWidget&) = delete;

  QScopedPointer<Ui::ThresholdSinkWidget> m_ui;
};
} // namespace tomviz
#endif
