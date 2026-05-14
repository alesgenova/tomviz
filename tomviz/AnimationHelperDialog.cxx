/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AnimationHelperDialog.h"
#include "ui_AnimationHelperDialog.h"

#include "ActiveObjects.h"
#include "ContourAnimation.h"
#include "SliceAnimation.h"
#include "Utilities.h"

#include "pipeline/Pipeline.h"
#include "pipeline/SourceNode.h"
#include "pipeline/sinks/ContourSink.h"
#include "pipeline/sinks/SliceSink.h"

#include <pqAnimationCue.h>
#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqPVApplicationCore.h>
#include <pqPropertyLinks.h>
#include <pqRenderView.h>
#include <pqSMAdaptor.h>
#include <pqSaveAnimationReaction.h>

#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>

namespace tomviz {

namespace {

pipeline::SourceNode* findUpstreamSource(pipeline::Node* node)
{
  if (!node) {
    return nullptr;
  }
  if (auto* src = qobject_cast<pipeline::SourceNode*>(node)) {
    return src;
  }
  for (auto* up : node->upstreamNodes()) {
    if (auto* src = findUpstreamSource(up)) {
      return src;
    }
  }
  return nullptr;
}

} // anonymous namespace

class AnimationHelperDialog::Internal : public QObject
{
public:
  Ui::AnimationHelperDialog ui;
  pqPropertyLinks pqLinks;
  QPointer<AnimationHelperDialog> parent;
  QList<QPointer<ModuleAnimation>> moduleAnimations;

  Internal(AnimationHelperDialog* p) : QObject(p), parent(p)
  {
    ui.setupUi(p);

    ui.modulesTabWidget->tabBar()->hide();

    updateGui();
    setupConnections();
  }

  void setupConnections()
  {
    // Camera animations
    connect(ui.clearCameraAnimations, &QPushButton::clicked, this,
            &Internal::clearCameraAnimations);
    connect(ui.createCameraOrbit, &QPushButton::clicked, this,
            &Internal::createCameraOrbitInternal);

    // Time series
    connect(&activeObjects(),
            &ActiveObjects::timeSeriesAnimationsEnableStateChanged,
            ui.enableTimeSeriesAnimations, &QCheckBox::setChecked);
    connect(ui.enableTimeSeriesAnimations, &QCheckBox::toggled,
            &activeObjects(), &ActiveObjects::enableTimeSeriesAnimations);
    connect(ui.enableTimeSeriesAnimations, &QCheckBox::toggled, this,
            [this](bool b) {
              updateEnableStates();
              if (b) {
                play();
              }
            });

    // Pipeline node changes
    if (auto* pip = pipeline()) {
      connect(pip, &pipeline::Pipeline::nodeAdded, this,
              &Internal::onNodeAdded);
      connect(pip, &pipeline::Pipeline::nodeRemoved, this,
              &Internal::onNodeRemoved);
    }

    // Sink selection
    connect(ui.selectedDataSource,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Internal::selectedDataSourceChanged);
    connect(ui.selectedModule,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Internal::selectedModuleChanged);
    connect(ui.addModuleAnimation, &QPushButton::clicked, this,
            &Internal::addModuleAnimation);
    connect(ui.clearModuleAnimations, &QPushButton::clicked, this,
            &Internal::clearModuleAnimations);

    // All animations
    pqLinks.addPropertyLink(ui.numberOfFrames, "value",
                            SIGNAL(valueChanged(int)), scene()->getProxy(),
                            scene()->getProxy()->GetProperty("NumberOfFrames"),
                            0);
    connect(ui.numberOfFrames, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &Internal::numberOfFramesModified);
    connect(ui.exportMovie, &QPushButton::clicked, this,
            &Internal::exportMovie);
    connect(ui.clearAllAnimations, &QPushButton::clicked, this,
            &Internal::clearAllAnimations);
  }

  void play() { scene()->getProxy()->InvokeCommand("Play"); }

  void updateGui()
  {
    ui.enableTimeSeriesAnimations->setChecked(
      activeObjects().timeSeriesAnimationsEnabled());

    updateDataSourceOptions();
    updateEnableStates();
  }

  void updateEnableStates()
  {
    cleanupNullAnimations();

    bool hasCameraAnimations = false;
    for (auto* cue : scene()->getCues()) {
      if (cue->getSMName().startsWith("CameraAnimationCue")) {
        hasCameraAnimations = true;
        break;
      }
    }

    bool hasTimeSeries = false;
    auto* tk = activeObjects().activeTimeKeeper();
    if (tk && !tk->getTimeSteps().empty()) {
      hasTimeSeries = true;
    }

    bool timeSeriesEnabled =
      ui.enableTimeSeriesAnimations->isChecked() && hasTimeSeries;

    bool hasDataSourceOptions = ui.selectedDataSource->count() != 0;
    bool hasModuleOptions = ui.selectedModule->count() != 0;
    bool moduleSelected = selectedSink() != nullptr;
    bool hasModuleAnimations = !moduleAnimations.empty();

    bool hasAnyAnimations =
      hasCameraAnimations || timeSeriesEnabled || hasModuleAnimations;

    ui.clearCameraAnimations->setEnabled(hasCameraAnimations);
    ui.enableTimeSeriesAnimations->setEnabled(hasTimeSeries);
    ui.addModuleAnimation->setEnabled(moduleSelected);
    ui.selectedDataSource->setEnabled(hasDataSourceOptions);
    ui.selectedModule->setEnabled(hasModuleOptions);
    ui.clearModuleAnimations->setEnabled(hasModuleAnimations);
    ui.exportMovie->setEnabled(hasAnyAnimations);
    ui.clearAllAnimations->setEnabled(hasAnyAnimations);
  }

  // Camera
  void clearCameraAnimations()
  {
    clearCameraCues();
    updateEnableStates();
  }

  void createCameraOrbitInternal()
  {
    auto* renderView = activeObjects().activePqRenderView();

    clearCameraCues(renderView->getRenderViewProxy());
    createCameraOrbit(renderView->getRenderViewProxy());

    updateEnableStates();
    play();
  }

  QStringList moduleTabTexts()
  {
    QStringList types;
    for (int i = 0; i < ui.modulesTabWidget->count(); ++i) {
      types.append(ui.modulesTabWidget->tabText(i));
    }
    return types;
  }

  // Data sources (pipeline source nodes)
  void updateDataSourceOptions()
  {
    QSignalBlocker blocked(ui.selectedDataSource);
    auto* previouslySelected = selectedSource();
    int previouslySelectedIndex = -1;

    ui.selectedDataSource->clear();

    auto* pip = pipeline();
    if (!pip) {
      updateEnableStates();
      return;
    }

    QStringList usedLabels;
    int idx = 0;
    for (auto* node : pip->nodes()) {
      auto* source = qobject_cast<pipeline::SourceNode*>(node);
      if (!source) {
        continue;
      }

      auto label = source->label();
      if (label.isEmpty()) {
        label = "Source";
      }

      auto uniqueLabel = label;
      int n = 1;
      while (usedLabels.contains(uniqueLabel)) {
        uniqueLabel = label + " " + QString::number(++n);
      }
      usedLabels.append(uniqueLabel);

      ui.selectedDataSource->addItem(
        uniqueLabel, QVariant::fromValue(static_cast<QObject*>(source)));

      if (source == previouslySelected) {
        previouslySelectedIndex = idx;
      }
      ++idx;
    }

    if (previouslySelectedIndex != -1) {
      ui.selectedDataSource->setCurrentIndex(previouslySelectedIndex);
    } else {
      selectedDataSourceChanged();
    }

    updateEnableStates();
  }

  pipeline::SourceNode* selectedSource()
  {
    if (ui.selectedDataSource->count() == 0) {
      return nullptr;
    }

    return qobject_cast<pipeline::SourceNode*>(
      ui.selectedDataSource->currentData().value<QObject*>());
  }

  // Sinks (animatable modules)
  void updateModuleOptions()
  {
    QSignalBlocker blocked(ui.selectedModule);
    auto* previouslySelected = selectedSink();
    int previouslySelectedIndex = -1;

    ui.selectedModule->clear();

    auto* source = selectedSource();
    auto* pip = pipeline();
    if (!source || !pip) {
      updateEnableStates();
      return;
    }

    QList<pipeline::Node*> sinks;
    for (auto* node : pip->nodes()) {
      if (qobject_cast<pipeline::ContourSink*>(node) ||
          qobject_cast<pipeline::SliceSink*>(node)) {
        if (findUpstreamSource(node) == source) {
          sinks.append(node);
        }
      }
    }

    QStringList labels;
    for (auto* sink : sinks) {
      auto label = sink->label();
      if (label.isEmpty()) {
        if (qobject_cast<pipeline::ContourSink*>(sink)) {
          label = "Contour";
        } else if (qobject_cast<pipeline::SliceSink*>(sink)) {
          label = "Slice";
        }
      }

      auto uniqueLabel = label;
      int n = 1;
      while (labels.contains(uniqueLabel)) {
        uniqueLabel = label + " " + QString::number(++n);
      }
      labels.append(uniqueLabel);
    }

    for (int i = 0; i < sinks.size(); ++i) {
      ui.selectedModule->addItem(
        labels[i], QVariant::fromValue(static_cast<QObject*>(sinks[i])));

      if (sinks[i] == previouslySelected) {
        previouslySelectedIndex = i;
      }
    }

    if (previouslySelectedIndex != -1) {
      ui.selectedModule->setCurrentIndex(previouslySelectedIndex);
    } else {
      selectedModuleChanged();
    }

    updateEnableStates();
  }

  pipeline::Node* selectedSink()
  {
    if (ui.selectedModule->count() == 0) {
      return nullptr;
    }

    return qobject_cast<pipeline::Node*>(
      ui.selectedModule->currentData().value<QObject*>());
  }

  void selectedDataSourceChanged() { updateModuleOptions(); }

  void selectedModuleChanged()
  {
    auto* node = selectedSink();
    int tabIndex = 0;

    if (auto* contour = qobject_cast<pipeline::ContourSink*>(node)) {
      tabIndex = moduleTabTexts().indexOf("Contour");
      if (tabIndex < 0) {
        tabIndex = 0;
      }
      ui.modulesTabWidget->setCurrentIndex(tabIndex);
      setupContourTab(contour);
    } else if (auto* slice = qobject_cast<pipeline::SliceSink*>(node)) {
      tabIndex = moduleTabTexts().indexOf("Slice");
      if (tabIndex < 0) {
        tabIndex = 0;
      }
      ui.modulesTabWidget->setCurrentIndex(tabIndex);
      setupSliceTab(slice);
    } else {
      ui.modulesTabWidget->setCurrentIndex(0);
    }

    updateEnableStates();
  }

  void onNodeAdded(pipeline::Node*)
  {
    QTimer::singleShot(0, this, [this]() {
      updateDataSourceOptions();
      updateModuleOptions();
    });
  }

  void onNodeRemoved(pipeline::Node*)
  {
    cleanupNullAnimations();
    updateDataSourceOptions();
    updateModuleOptions();
    updateEnableStates();
  }

  void setupContourTab(pipeline::ContourSink* sink)
  {
    double range[2];
    sink->scalarRange(range);

    ui.contourStart->setMinimum(range[0]);
    ui.contourStart->setMaximum(range[1]);
    ui.contourStop->setMinimum(range[0]);
    ui.contourStop->setMaximum(range[1]);

    ui.contourStart->setValue((range[1] - range[0]) / 3 + range[0]);
    ui.contourStop->setValue((range[1] - range[0]) * 2 / 3 + range[0]);
  }

  void setupSliceTab(pipeline::SliceSink* sink)
  {
    double max = sink->maxSlice();

    ui.sliceStart->setMinimum(0);
    ui.sliceStart->setMaximum(max);
    ui.sliceStop->setMinimum(0);
    ui.sliceStop->setMaximum(max);

    ui.sliceStart->setValue(0);
    ui.sliceStop->setValue(max);

    connect(sink, &pipeline::SliceSink::directionChanged, this,
            [this, sink]() {
              if (sink != this->selectedSink()) {
                disconnect(sink, nullptr, this, nullptr);
                return;
              }
              this->setupSliceTab(sink);
            });
  }

  void addModuleAnimation()
  {
    auto* node = selectedSink();
    if (!node) {
      return;
    }

    for (int i = 0; i < moduleAnimations.size(); ++i) {
      if (!moduleAnimations[i] || node == moduleAnimations[i]->baseNode) {
        if (moduleAnimations[i]) {
          moduleAnimations[i]->deleteLater();
        }
        moduleAnimations.removeAt(i);
        --i;
      }
    }

    if (qobject_cast<pipeline::ContourSink*>(node)) {
      addContourAnimation();
    } else if (qobject_cast<pipeline::SliceSink*>(node)) {
      addSliceAnimation();
    }

    updateEnableStates();
    play();
  }

  void addContourAnimation()
  {
    auto start = ui.contourStart->value();
    auto stop = ui.contourStop->value();
    auto* sink = qobject_cast<pipeline::ContourSink*>(selectedSink());
    moduleAnimations.append(new ContourAnimation(sink, start, stop));
  }

  void addSliceAnimation()
  {
    auto start = ui.sliceStart->value();
    auto stop = ui.sliceStop->value();
    auto* sink = qobject_cast<pipeline::SliceSink*>(selectedSink());
    moduleAnimations.append(new SliceAnimation(sink, start, stop));
  }

  void clearModuleAnimations()
  {
    for (auto animation : moduleAnimations) {
      if (animation) {
        animation->deleteLater();
      }
    }

    moduleAnimations.clear();

    updateEnableStates();
  }

  void cleanupNullAnimations()
  {
    for (int i = moduleAnimations.size() - 1; i >= 0; --i) {
      if (moduleAnimations[i].isNull()) {
        moduleAnimations.removeAt(i);
      }
    }
  }

  // All animations
  void numberOfFramesModified()
  {
    pqSMAdaptor::setEnumerationProperty(
      scene()->getProxy()->GetProperty("PlayMode"), "Sequence");
  }

  void exportMovie() { pqSaveAnimationReaction::saveAnimation(); }

  void clearAllAnimations()
  {
    clearCameraAnimations();
    if (ui.enableTimeSeriesAnimations->isEnabled()) {
      ui.enableTimeSeriesAnimations->setChecked(false);
    }
    clearModuleAnimations();

    updateEnableStates();
  }

  ActiveObjects& activeObjects() { return ActiveObjects::instance(); }

  pipeline::Pipeline* pipeline()
  {
    return activeObjects().pipeline();
  }

  pqAnimationScene* scene()
  {
    return pqPVApplicationCore::instance()
      ->animationManager()
      ->getActiveScene();
  }
}; // end class Internal

AnimationHelperDialog::AnimationHelperDialog(QWidget* parent)
  : QDialog(parent), m_internal(new Internal(this))
{
}

AnimationHelperDialog::~AnimationHelperDialog() = default;

} // namespace tomviz
