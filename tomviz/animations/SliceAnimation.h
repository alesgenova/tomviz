/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSliceAnimation_h
#define tomvizSliceAnimation_h

#include "ModuleAnimation.h"

#include "pipeline/sinks/SliceSink.h"

namespace tomviz {

class SliceAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  double startValue = 0;
  double stopValue = 0;

  SliceAnimation(pipeline::SliceSink* sink, double start, double stop)
    : ModuleAnimation(sink), startValue(start), stopValue(stop)
  {
  }

  pipeline::SliceSink* sink()
  {
    return qobject_cast<pipeline::SliceSink*>(baseNode.data());
  }

  void onTimeChanged() override
  {
    if (!timeKeeper() || !sink()) {
      return;
    }

    double value = (stopValue - startValue) * progress() + startValue;
    sink()->setSlice(static_cast<int>(value));
  }
};

} // namespace tomviz

#endif
