/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineCropTransform_h
#define tomvizPipelineCropTransform_h

#include "TransformNode.h"

namespace tomviz {
namespace pipeline {

/// Transform that crops a volume to a sub-region specified by bounds.
class CropTransform : public TransformNode
{
  Q_OBJECT

public:
  CropTransform(QObject* parent = nullptr);
  ~CropTransform() override = default;

  void setCropBounds(const int bounds[6]);
  const int* cropBounds() const { return m_bounds; }

  QIcon icon() const override;

  bool hasPropertiesWidget() const override;
  EditNodeWidget* createPropertiesWidget(Pipeline* pipeline,
                                         QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override;

private:
  int m_bounds[6];
};

} // namespace pipeline
} // namespace tomviz

#endif
