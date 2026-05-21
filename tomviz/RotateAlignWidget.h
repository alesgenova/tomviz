/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizRotateAlignWidget_h
#define tomvizRotateAlignWidget_h

#include "CustomPythonNodeWidget.h"
#include "PortData.h"

#include "vtkSmartPointer.h"

#include <QMap>
#include <QScopedPointer>
#include <QString>

class vtkImageData;
class vtkSMProxy;

namespace tomviz {

class RotateAlignWidget : public pipeline::CustomPythonNodeWidget
{
  Q_OBJECT

public:
  RotateAlignWidget(const QMap<QString, pipeline::PortData>& inputs,
                    QWidget* parent = nullptr);
  ~RotateAlignWidget();

  void getValues(QMap<QString, QVariant>& map) override;
  void setValues(const QMap<QString, QVariant>& map) override;

public slots:
  bool eventFilter(QObject* o, QEvent* e) override;

signals:
  void creatingAlignedData();

protected slots:
  void onSumProjectionsToggled(bool);
  void onProjectionNumberChanged(int);
  void onRotationShiftChanged(int);
  void onRotationAngleChanged(double);
  void onRotationAxisChanged();
  void onOrientationChanged(int);

  void updateWidgets();
  void updateControls();

  void onFinalReconButtonPressed();

  void showChangeColorMapDialog0() { this->showChangeColorMapDialog(0); }
  void showChangeColorMapDialog1() { this->showChangeColorMapDialog(1); }
  void showChangeColorMapDialog2() { this->showChangeColorMapDialog(2); }

  void changeColorMap0() { this->changeColorMap(0); }
  void changeColorMap1() { this->changeColorMap(1); }
  void changeColorMap2() { this->changeColorMap(2); }

private:
  void initUI(vtkSMProxy* sourceColorMap);
  void onReconSliceChanged(int idx, int val);
  void showChangeColorMapDialog(int reconSlice);
  void changeColorMap(int reconSlice);

private:
  Q_DISABLE_COPY(RotateAlignWidget)

  class RAWInternal;
  QScopedPointer<RAWInternal> Internals;
};
} // namespace tomviz
#endif
