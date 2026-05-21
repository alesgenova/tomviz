/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AddAlignReaction.h"

#include "TransformUtils.h"
#include "pipeline/transforms/TranslateAlignTransform.h"

namespace tomviz {

AddAlignReaction::AddAlignReaction(QAction* parentObject)
  : Reaction(parentObject)
{
}

void AddAlignReaction::align(DataSource*)
{
  auto* transform = new pipeline::TranslateAlignTransform();
  insertTransformIntoPipeline(transform);
}
} // namespace tomviz
