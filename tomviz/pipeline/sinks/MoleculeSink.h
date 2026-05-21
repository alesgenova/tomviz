/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineMoleculeSink_h
#define tomvizPipelineMoleculeSink_h

#include "LegacyModuleSink.h"

#include <QPointer>
#include <vtkNew.h>

class vtkActor;
class vtkMoleculeMapper;

namespace tomviz {
class DoubleSliderWidget;
}

namespace tomviz {
namespace pipeline {

/// Molecular structure visualization sink using ball-and-stick rendering.
/// Full implementation requires a Molecule data type in the pipeline;
/// currently accepts molecule port data and renders via vtkMoleculeMapper.
class MoleculeSink : public LegacyModuleSink
{
  Q_OBJECT

public:
  MoleculeSink(QObject* parent = nullptr);
  ~MoleculeSink() override;

  QIcon icon() const override;

  void setVisibility(bool visible) override;

  bool initialize(vtkSMViewProxy* view) override;
  bool finalize() override;

  void clearVisualization() override;

  QWidget* createSinkPropertiesWidget(QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  double ballRadius() const;
  void setBallRadius(double radius);

  double bondRadius() const;
  void setBondRadius(double radius);

  void onMetadataChanged() override;

protected:
  bool consume(const QMap<QString, PortData>& inputs) override;

private:
  vtkNew<vtkMoleculeMapper> m_moleculeMapper;
  vtkNew<vtkActor> m_actor;
  QPointer<DoubleSliderWidget> m_ballSlider;
  QPointer<DoubleSliderWidget> m_stickSlider;
};

} // namespace pipeline
} // namespace tomviz

#endif
