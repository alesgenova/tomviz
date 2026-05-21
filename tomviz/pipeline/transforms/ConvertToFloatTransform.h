/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineConvertToFloatTransform_h
#define tomvizPipelineConvertToFloatTransform_h

#include "TransformNode.h"

namespace tomviz {
namespace pipeline {

/// Transform that converts image scalars to float type.
class ConvertToFloatTransform : public TransformNode
{
  Q_OBJECT

public:
  ConvertToFloatTransform(QObject* parent = nullptr);
  ~ConvertToFloatTransform() override = default;

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override;
};

} // namespace pipeline
} // namespace tomviz

#endif
