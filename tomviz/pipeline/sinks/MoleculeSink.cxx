/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "MoleculeSink.h"

#include "DoubleSliderWidget.h"
#include "data/VolumeData.h"

#include <vtkActor.h>
#include <vtkMolecule.h>
#include <vtkMoleculeMapper.h>
#include <vtkPVRenderView.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

#include <QFormLayout>
#include <QSignalBlocker>

namespace tomviz {
namespace pipeline {

MoleculeSink::MoleculeSink(QObject* parent) : LegacyModuleSink(parent)
{
  addInput("molecule", PortType::Molecule);
  setLabel("Molecule");

  m_actor->SetMapper(m_moleculeMapper);
  m_actor->SetVisibility(0);
}

MoleculeSink::~MoleculeSink()
{
  finalize();
}

QIcon MoleculeSink::icon() const
{
  return QIcon(QStringLiteral(":/pqWidgets/Icons/pqGroup.svg"));
}

void MoleculeSink::setVisibility(bool visible)
{
  m_actor->SetVisibility(visible ? 1 : 0);
  LegacyModuleSink::setVisibility(visible);
}

bool MoleculeSink::initialize(vtkSMViewProxy* view)
{
  if (!LegacyModuleSink::initialize(view)) {
    return false;
  }

  renderView()->GetRenderer()->AddActor(m_actor);
  return true;
}

bool MoleculeSink::finalize()
{
  if (renderView()) {
    renderView()->GetRenderer()->RemoveActor(m_actor);
  }
  return LegacyModuleSink::finalize();
}

void MoleculeSink::clearVisualization()
{
  m_actor->SetVisibility(0);
}

bool MoleculeSink::consume(const QMap<QString, PortData>& inputs)
{
  if (!validateInput(inputs, "molecule")) {
    return false;
  }

  // Extract vtkMolecule from PortData and connect to mapper
  try {
    auto molecule =
      inputs["molecule"].value<vtkSmartPointer<vtkMolecule>>();
    if (molecule) {
      m_moleculeMapper->SetInputData(molecule);
    }
  } catch (const std::bad_any_cast&) {
    // PortData doesn't contain vtkSmartPointer<vtkMolecule> yet
  }

  m_actor->SetVisibility(visibility() ? 1 : 0);

  onMetadataChanged();
  emit renderNeeded();
  return true;
}

double MoleculeSink::ballRadius() const
{
  return m_moleculeMapper->GetAtomicRadiusScaleFactor();
}

void MoleculeSink::setBallRadius(double radius)
{
  m_moleculeMapper->SetAtomicRadiusScaleFactor(radius);
  emit renderNeeded();
}

double MoleculeSink::bondRadius() const
{
  return m_moleculeMapper->GetBondRadius();
}

void MoleculeSink::setBondRadius(double radius)
{
  m_moleculeMapper->SetBondRadius(radius);
  emit renderNeeded();
}

QWidget* MoleculeSink::createSinkPropertiesWidget(QWidget* parent)
{
  auto* widget = new QWidget(parent);
  auto* layout = new QFormLayout;
  widget->setLayout(layout);

  m_ballSlider = new DoubleSliderWidget(true);
  m_ballSlider->setLineEditWidth(50);
  m_ballSlider->setMaximum(4.0);
  m_ballSlider->setValue(ballRadius());
  layout->addRow("Ball Radius", m_ballSlider);

  m_stickSlider = new DoubleSliderWidget(true);
  m_stickSlider->setLineEditWidth(50);
  m_stickSlider->setMaximum(2.0);
  m_stickSlider->setValue(bondRadius());
  layout->addRow("Stick Radius", m_stickSlider);

  connect(m_ballSlider, &DoubleSliderWidget::valueEdited, this,
          [this](double val) { setBallRadius(val); });
  connect(m_stickSlider, &DoubleSliderWidget::valueEdited, this,
          [this](double val) { setBondRadius(val); });

  return widget;
}

QJsonObject MoleculeSink::serialize() const
{
  auto json = LegacyModuleSink::serialize();
  json["ballRadius"] = m_moleculeMapper->GetAtomicRadiusScaleFactor();
  json["stickRadius"] = m_moleculeMapper->GetBondRadius();
  return json;
}

bool MoleculeSink::deserialize(const QJsonObject& json)
{
  if (!LegacyModuleSink::deserialize(json)) {
    return false;
  }
  if (json.contains("ballRadius")) {
    setBallRadius(json["ballRadius"].toDouble());
  }
  if (json.contains("stickRadius")) {
    setBondRadius(json["stickRadius"].toDouble());
  }
  return true;
}

void MoleculeSink::onMetadataChanged()
{
  auto vol = volumeData();
  if (!vol) return;
  auto pos = vol->displayPosition();
  auto orient = vol->displayOrientation();
  m_actor->SetPosition(pos.data());
  m_actor->SetOrientation(orient.data());
  emit renderNeeded();
}

} // namespace pipeline
} // namespace tomviz
