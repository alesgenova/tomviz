/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizContourSinkWidget_h
#define tomvizContourSinkWidget_h

#include <QScopedPointer>
#include <QWidget>

/**
 * \brief UI layer of ContourSink.
 *
 * Signals are forwarded to ContourSink. This class is intended to contain only
 * logic related to UI actions.
 */

namespace Ui {
class ContourSinkWidget;
class LightingParametersForm;
} // namespace Ui

namespace tomviz {

class ContourSinkWidget : public QWidget
{
  Q_OBJECT

public:
  ContourSinkWidget(QWidget* parent_ = nullptr);
  ~ContourSinkWidget() override;

  void setIsoRange(double range[2]);
  void setContourByArrayOptions(const QStringList& scalars,
                                int activeScalar);
  void setColorByArrayOptions(const QStringList& options);

  //@{
  /**
   * UI update methods. The actual model state is stored in ContourSink for
   * these parameters, so the UI needs to be updated if the state changes or
   * when constructing the UI.
   */
  void setColorMapData(const bool state);
  void setAmbient(const double value);
  void setDiffuse(const double value);
  void setSpecular(const double value);
  void setSpecularPower(const double value);
  void setIso(const double value);
  void setRepresentation(const QString& representation);
  void setOpacity(const double value);
  void setColor(const QColor& color);
  void setUseSolidColor(const bool state);
  void setContourByArrayValue(int i);
  void setColorByArray(const bool state);
  void setColorByArrayName(const QString& name);
  //@}

signals:
  //@{
  /**
   * Forwarded signals.
   */
  void colorMapDataToggled(const bool state);
  void ambientChanged(const double value);
  void diffuseChanged(const double value);
  void specularChanged(const double value);
  void specularPowerChanged(const double value);
  void isoChanged(const double value);
  void representationChanged(const QString& representation);
  void opacityChanged(const double value);
  void colorChanged(const QColor& color);
  void useSolidColorToggled(const bool state);
  void contourByArrayValueChanged(int i);
  void colorByArrayToggled(const bool state);
  void colorByArrayNameChanged(const QString& name);
  //@}

private:
  void onContourByArrayIndexChanged(int i);
  void onColorByArrayIndexChanged(int i);
  void onRepresentationIndexChanged(int i);

  ContourSinkWidget(const ContourSinkWidget&) = delete;
  void operator=(const ContourSinkWidget&) = delete;

  QScopedPointer<Ui::ContourSinkWidget> m_ui;
  QScopedPointer<Ui::LightingParametersForm> m_uiLighting;
};
} // namespace tomviz
#endif
