/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ReconstructionReaction.h"

#include "TransformUtils.h"
#include "pipeline/transforms/ReconstructionTransform.h"

namespace tomviz {

ReconstructionReaction::ReconstructionReaction(QAction* parentObject)
  : Reaction(parentObject)
{
  setAcceptedInputTypes(pipeline::PortType::TiltSeries);
}

void ReconstructionReaction::recon(DataSource*)
{
  auto* transform = new pipeline::ReconstructionTransform();
  insertTransformIntoPipeline(transform);
}
} // namespace tomviz
