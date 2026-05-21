/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizBehaviors_h
#define tomvizBehaviors_h

#include <QObject>
#include <QScopedPointer>

class QMainWindow;

namespace tomviz {

class TimeSeriesLabel;

/// Behaviors instantiates tomviz relevant ParaView behaviors (and any new
/// ones) as needed.

class Behaviors : public QObject
{
  Q_OBJECT

public:
  Behaviors(QMainWindow* mainWindow);

private:
  Q_DISABLE_COPY(Behaviors)

  QScopedPointer<TimeSeriesLabel> m_timeSeriesLabel;

  void registerCustomOperatorUIs();
};
} // namespace tomviz
#endif
