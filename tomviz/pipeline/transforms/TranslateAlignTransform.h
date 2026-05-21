/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineTranslateAlignTransform_h
#define tomvizPipelineTranslateAlignTransform_h

#include "TransformNode.h"

#include <vtkVector.h>

#include <QVector>

namespace tomviz {
namespace pipeline {

/// Transform that applies per-slice 2D translation offsets to align a
/// tilt series or image stack.
class TranslateAlignTransform : public TransformNode
{
  Q_OBJECT

public:
  TranslateAlignTransform(QObject* parent = nullptr);
  ~TranslateAlignTransform() override = default;

  void setAlignOffsets(const QVector<vtkVector2i>& offsets);
  void setDraftAlignOffsets(const QVector<vtkVector2i>& offsets);
  const QVector<vtkVector2i>& getAlignOffsets() const { return m_offsets; }
  const QVector<vtkVector2i>& getDraftAlignOffsets() const
  {
    return m_draftOffsets;
  }

  bool hasPropertiesWidget() const override;
  EditNodeWidget* createPropertiesWidget(Pipeline* pipeline,
                                         QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override;

private:
  QVector<vtkVector2i> m_offsets;
  QVector<vtkVector2i> m_draftOffsets;
};

} // namespace pipeline
} // namespace tomviz

#endif
