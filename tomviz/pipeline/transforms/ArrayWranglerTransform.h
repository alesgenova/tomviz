/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineArrayWranglerTransform_h
#define tomvizPipelineArrayWranglerTransform_h

#include "TransformNode.h"

namespace tomviz {
namespace pipeline {

/// Transform that converts array data to UInt8 or UInt16, optionally
/// extracting a single component from multi-component arrays.
class ArrayWranglerTransform : public TransformNode
{
  Q_OBJECT

public:
  ArrayWranglerTransform(QObject* parent = nullptr);
  ~ArrayWranglerTransform() override = default;

  enum class OutputType
  {
    UInt8,
    UInt16
  };

  void setOutputType(OutputType t) { m_outputType = t; }
  OutputType outputType() const { return m_outputType; }

  void setComponentToKeep(int i) { m_componentToKeep = i; }
  int componentToKeep() const { return m_componentToKeep; }

  bool hasPropertiesWidget() const override;
  EditNodeWidget* createPropertiesWidget(Pipeline* pipeline,
                                         QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override;

private:
  OutputType m_outputType = OutputType::UInt8;
  int m_componentToKeep = 0;
};

} // namespace pipeline
} // namespace tomviz

#endif
