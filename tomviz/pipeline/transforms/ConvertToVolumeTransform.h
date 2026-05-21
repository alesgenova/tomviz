/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineConvertToVolumeTransform_h
#define tomvizPipelineConvertToVolumeTransform_h

#include "TransformNode.h"

namespace tomviz {
namespace pipeline {

/// Pass-through transform that re-types input data as a Volume.
class ConvertToVolumeTransform : public TransformNode
{
  Q_OBJECT

public:
  ConvertToVolumeTransform(QObject* parent = nullptr);
  ~ConvertToVolumeTransform() override = default;

  void setOutputType(PortType type);
  PortType outputType() const;

  void setOutputLabel(const QString& label);
  QString outputLabel() const;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override;

private:
  PortType m_outputType = PortType::Volume;
  QString m_outputLabel = "Mark as Volume";
};

} // namespace pipeline
} // namespace tomviz

#endif
