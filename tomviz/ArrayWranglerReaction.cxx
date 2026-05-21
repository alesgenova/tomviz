/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ArrayWranglerReaction.h"

#include "TransformUtils.h"
#include "pipeline/transforms/ArrayWranglerTransform.h"

namespace tomviz {

ArrayWranglerReaction::ArrayWranglerReaction(QAction* parentObject,
                                             QMainWindow* mw)
  : Reaction(parentObject), m_mainWindow(mw)
{
}

void ArrayWranglerReaction::wrangleArray(DataSource*)
{
  auto* transform = new pipeline::ArrayWranglerTransform();
  insertTransformIntoPipeline(transform);
}
} // namespace tomviz
