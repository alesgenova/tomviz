/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPtychoWidget_h
#define tomvizPtychoWidget_h

#include "CustomPythonNodeWidget.h"
#include "PortData.h"

#include <QMap>
#include <QScopedPointer>
#include <QString>

namespace tomviz {

class PtychoWidget : public pipeline::CustomPythonNodeWidget
{
  Q_OBJECT

public:
  PtychoWidget(const QMap<QString, pipeline::PortData>& inputs,
               QWidget* parent = nullptr);
  ~PtychoWidget() override;

  void getValues(QMap<QString, QVariant>& map) override;
  void setValues(const QMap<QString, QVariant>& map) override;
  void writeSettings() override;

private:
  class Internal;
  QScopedPointer<Internal> m_internal;
};

} // namespace tomviz

#endif
