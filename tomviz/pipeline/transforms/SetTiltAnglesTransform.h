/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineSetTiltAnglesTransform_h
#define tomvizPipelineSetTiltAnglesTransform_h

#include "TransformNode.h"

#include <QMap>
#include <QVector>

namespace tomviz {
namespace pipeline {

/// Transform that sets tilt angles on a VolumeData.
class SetTiltAnglesTransform : public TransformNode
{
  Q_OBJECT

public:
  SetTiltAnglesTransform(QObject* parent = nullptr);
  ~SetTiltAnglesTransform() override = default;

  void setTiltAngles(const QMap<size_t, double>& angles);
  QMap<size_t, double> tiltAnglesMap() const;

  void setScanIds(const QVector<int>& ids);
  QVector<int> scanIds() const;

  bool hasPropertiesWidget() const override;
  bool propertiesWidgetNeedsInput() const override;
  EditTransformWidget* createPropertiesWidget(QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override;

private:
  QMap<size_t, double> m_tiltAngles;
  QVector<int> m_scanIds;
};

} // namespace pipeline
} // namespace tomviz

#endif
