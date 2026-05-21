/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TransposeDataReaction.h"

#include "TransformUtils.h"
#include "pipeline/transforms/TransposeDataTransform.h"

namespace tomviz {

TransposeDataReaction::TransposeDataReaction(QAction* parentObject,
                                             QMainWindow* mw)
  : Reaction(parentObject), m_mainWindow(mw)
{
}

void TransposeDataReaction::transposeData(DataSource*)
{
  auto* transform = new pipeline::TransposeDataTransform();
  insertTransformIntoPipeline(transform);
}
} // namespace tomviz
