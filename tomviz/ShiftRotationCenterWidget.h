/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizShiftRotationCenterWidget_h
#define tomvizShiftRotationCenterWidget_h

#include "CustomPythonNodeWidget.h"
#include "PortData.h"

#include <vtkSmartPointer.h>

#include <QMap>
#include <QScopedPointer>
#include <QString>

class vtkImageData;
class vtkSMProxy;

namespace tomviz {

class ShiftRotationCenterWidget : public pipeline::CustomPythonNodeWidget
{
  Q_OBJECT

public:
  ShiftRotationCenterWidget(
    const QMap<QString, pipeline::PortData>& inputs,
    QWidget* parent = nullptr);
  ~ShiftRotationCenterWidget();

  void getValues(QMap<QString, QVariant>& map) override;
  void setValues(const QMap<QString, QVariant>& map) override;

  void setScript(const QString& script) override;

  void writeSettings() override;

private:
  Q_DISABLE_COPY(ShiftRotationCenterWidget)

  class Internal;
  QScopedPointer<Internal> m_internal;
};

} // namespace tomviz
#endif
