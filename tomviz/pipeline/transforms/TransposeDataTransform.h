/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineTransposeDataTransform_h
#define tomvizPipelineTransposeDataTransform_h

#include "TransformNode.h"

namespace tomviz {
namespace pipeline {

/// Transform that transposes array axes between C and Fortran ordering.
class TransposeDataTransform : public TransformNode
{
  Q_OBJECT

public:
  TransposeDataTransform(QObject* parent = nullptr);
  ~TransposeDataTransform() override = default;

  enum class TransposeType
  {
    C,
    Fortran
  };

  void setTransposeType(TransposeType t) { m_transposeType = t; }
  TransposeType transposeType() const { return m_transposeType; }

  bool hasPropertiesWidget() const override;
  EditNodeWidget* createPropertiesWidget(Pipeline* pipeline,
                                         QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override;

private:
  TransposeType m_transposeType = TransposeType::C;
};

} // namespace pipeline
} // namespace tomviz

#endif
