/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizManualManipulationWidget_h
#define tomvizManualManipulationWidget_h

#include "CustomPythonNodeWidget.h"
#include "PortData.h"

#include <vtkSmartPointer.h>

#include <QMap>
#include <QScopedPointer>
#include <QString>

class vtkImageData;
class vtkSMProxy;

namespace tomviz {

class ManualManipulationWidget : public pipeline::CustomPythonNodeWidget
{
  Q_OBJECT

public:
  ManualManipulationWidget(const QMap<QString, pipeline::PortData>& inputs,
                           QWidget* parent = nullptr);
  ~ManualManipulationWidget();

  void getValues(QMap<QString, QVariant>& map) override;
  void setValues(const QMap<QString, QVariant>& map) override;

private:
  Q_DISABLE_COPY(ManualManipulationWidget)
  class Internal;
  QScopedPointer<Internal> m_internal;
};

} // namespace tomviz
#endif
