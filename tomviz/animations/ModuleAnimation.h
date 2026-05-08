/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleAnimation_h
#define tomvizModuleAnimation_h

#include "ActiveObjects.h"
#include "pipeline/Node.h"

#include <pqTimeKeeper.h>

#include <QObject>
#include <QPointer>

namespace tomviz {

class ModuleAnimation : public QObject
{
  Q_OBJECT

public:
  QPointer<pipeline::Node> baseNode;

  ModuleAnimation(pipeline::Node* node) : baseNode(node)
  {
    setupConnections();
  }

  virtual void setupConnections()
  {
    if (timeKeeper()) {
      connect(timeKeeper(), &pqTimeKeeper::timeChanged, this,
              &ModuleAnimation::onTimeChanged);
    }

    if (baseNode) {
      connect(baseNode.data(), &QObject::destroyed, this,
              &QObject::deleteLater);
    }
  }

  virtual ActiveObjects& activeObjects() { return ActiveObjects::instance(); }
  virtual pqTimeKeeper* timeKeeper()
  {
    return activeObjects().activeTimeKeeper();
  }

  virtual double time()
  {
    if (!timeKeeper()) {
      return 0;
    }

    return timeKeeper()->getTime();
  }

  virtual QList<double> timeSteps()
  {
    if (!timeKeeper()) {
      return { 0 };
    }

    auto timeSteps = timeKeeper()->getTimeSteps();
    if (timeSteps.empty()) {
      timeSteps.append(0);
      timeSteps.append(1);
    }

    return timeSteps;
  }

  virtual double timeStart() { return timeSteps().front(); }
  virtual double timeStop() { return timeSteps().back(); }
  virtual double progress() { return time() / (timeStop() - timeStart()); }

  virtual void onTimeChanged() {}
};

} // namespace tomviz

#endif
