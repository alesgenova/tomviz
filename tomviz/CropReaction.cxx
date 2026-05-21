/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "CropReaction.h"

#include "TransformUtils.h"
#include "pipeline/transforms/CropTransform.h"

namespace tomviz {

CropReaction::CropReaction(QAction* parentObject, QMainWindow* mw)
  : Reaction(parentObject), m_mainWindow(mw)
{
}

void CropReaction::crop(DataSource*)
{
  auto* transform = new pipeline::CropTransform();
  insertTransformIntoPipeline(transform);
}
} // namespace tomviz
