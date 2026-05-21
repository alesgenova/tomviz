/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ConvertToFloatReaction.h"

#include "TransformUtils.h"
#include "pipeline/transforms/ConvertToFloatTransform.h"

namespace tomviz {

ConvertToFloatReaction::ConvertToFloatReaction(QAction* parentObject)
  : Reaction(parentObject)
{
}

void ConvertToFloatReaction::convertToFloat()
{
  auto* transform = new pipeline::ConvertToFloatTransform();
  insertTransformIntoPipeline(transform);
}
} // namespace tomviz
