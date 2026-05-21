/* Manual demo app for pipeline Sinks with live visualization.
   Build target: pipelineSinkDemo

   Pipeline:  SphereSource -> ThresholdTransform -> [selected sink]
   The sink is displayed in a ParaView render view (or chart view for Plot).
   A PipelineStripWidget in a left dock shows the pipeline graph.
*/

#include "Pipeline.h"
#include "PipelineStripWidget.h"
#include "sources/SphereSource.h"
#include "transforms/ThresholdTransform.h"

// Sinks
#include "sinks/ClipSink.h"
#include "sinks/ContourSink.h"
#include "sinks/OutlineSink.h"
#include "sinks/RulerSink.h"
#include "sinks/ScaleCubeSink.h"
#include "sinks/SegmentSink.h"
#include "sinks/SliceSink.h"
#include "sinks/ThresholdSink.h"
#include "sinks/VolumeSink.h"

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqServerResource.h>
#include <pqView.h>
#include <vtkSMViewProxy.h>

#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>
#include <QVBoxLayout>

using namespace tomviz::pipeline;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

struct SinkEntry
{
  QString name;
  // Factory that creates a fresh sink each time
  std::function<LegacyModuleSink*()> create;
};

static QList<SinkEntry> sinkEntries()
{
  return {
    { "Outline", [] { return new OutlineSink(); } },
    { "Contour", [] { return new ContourSink(); } },
    { "Slice", [] { return new SliceSink(); } },
    { "Volume", [] { return new VolumeSink(); } },
    { "Threshold", [] { return new ThresholdSink(); } },
    { "Segment", [] { return new SegmentSink(); } },
    { "Clip", [] { return new ClipSink(); } },
    { "Ruler", [] { return new RulerSink(); } },
    { "Scale Cube", [] { return new ScaleCubeSink(); } },
  };
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  // ParaView application core — sets up the server-manager infrastructure
  pqPVApplicationCore pvCore(argc, argv);
  auto* builder = pqApplicationCore::instance()->getObjectBuilder();
  builder->createServer(pqServerResource("builtin:"));
  auto* server = pqApplicationCore::instance()->getActiveServer();

  // Create a 3D render view
  auto* pqRenderView = builder->createView("RenderView", server);
  auto* viewProxy = pqRenderView->getViewProxy();

  // -----------------------------------------------------------------------
  // Build the initial pipeline:  Sphere -> Threshold -> Outline
  // -----------------------------------------------------------------------
  auto entries = sinkEntries();

  auto* pipeline = new Pipeline();

  auto* source = new SphereSource();
  source->setDimensions(64, 64, 64);
  pipeline->addNode(source);

  auto* transform = new ThresholdTransform();
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("volume"));

  auto* sink = entries[0].create();
  pipeline->addNode(sink);
  pipeline->createLink(transform->outputPort("mask"),
                       sink->inputPort("volume"));

  // Execute the source so downstream data is available
  source->execute();
  // Execute the full pipeline (threshold + sink)
  pipeline->execute();

  // Initialize the sink with the render view so it creates its VTK actors
  sink->initialize(viewProxy);

  // Re-consume now that the view is set up
  // (the first execute ran without a view; the sink stored the data
  //  but could not create VTK actors.)
  pipeline->execute();

  // -----------------------------------------------------------------------
  // Window layout
  // -----------------------------------------------------------------------
  QMainWindow window;
  window.setWindowTitle("Pipeline Sink Demo");
  window.resize(1024, 768);

  // Central area: render view widget + controls on top
  auto* central = new QWidget();
  auto* centralLayout = new QVBoxLayout(central);
  centralLayout->setContentsMargins(0, 0, 0, 0);

  // Top toolbar: sink selector + execute button
  auto* toolbar = new QWidget();
  auto* toolbarLayout = new QHBoxLayout(toolbar);
  toolbarLayout->setContentsMargins(4, 4, 4, 4);

  auto* sinkLabel = new QLabel("Sink:");
  auto* sinkCombo = new QComboBox();
  for (auto& entry : entries) {
    sinkCombo->addItem(entry.name);
  }
  auto* executeBtn = new QPushButton("Execute");

  toolbarLayout->addWidget(sinkLabel);
  toolbarLayout->addWidget(sinkCombo);
  toolbarLayout->addWidget(executeBtn);
  toolbarLayout->addStretch();

  centralLayout->addWidget(toolbar);

  // Embed the ParaView render-view widget
  QWidget* viewWidget = pqRenderView->widget();
  centralLayout->addWidget(viewWidget, 1);

  window.setCentralWidget(central);

  // Left dock: pipeline strip widget
  auto* dock = new QDockWidget("Pipeline", &window);
  dock->setMinimumWidth(200);

  auto* scroll = new QScrollArea();
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* strip = new PipelineStripWidget();
  strip->setPipeline(pipeline);
  scroll->setWidget(strip);
  dock->setWidget(scroll);
  window.addDockWidget(Qt::LeftDockWidgetArea, dock);

  // Status bar
  window.statusBar()->showMessage("Ready");

  // -----------------------------------------------------------------------
  // Connections
  // -----------------------------------------------------------------------

  // When the sink combo changes, rebuild the pipeline tail
  QObject::connect(
    sinkCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
    [&](int index) {
      // Finalize and remove old sink
      sink->finalize();
      pipeline->removeNode(sink);
      // sink is deleted by removeNode (Pipeline owns its nodes)

      // Create new sink and wire it in
      sink = entries[index].create();
      pipeline->addNode(sink);
      pipeline->createLink(transform->outputPort("mask"),
                           sink->inputPort("volume"));

      // Initialize with the view and execute
      sink->initialize(viewProxy);
      pipeline->execute();

      strip->setPipeline(pipeline);
      pqRenderView->render();

      window.statusBar()->showMessage(
        QString("Switched to %1 sink").arg(entries[index].name));
    });

  // Execute button re-runs the pipeline
  QObject::connect(executeBtn, &QPushButton::clicked, [&]() {
    source->execute();
    pipeline->execute();
    pqRenderView->render();
    window.statusBar()->showMessage("Pipeline executed");
  });

  // Node selection feedback
  QObject::connect(strip, &PipelineStripWidget::nodeSelected, [&](Node* node) {
    if (node) {
      window.statusBar()->showMessage(
        QString("Selected: %1").arg(node->label()));
    }
  });

  // Render when any sink signals it needs a render
  QObject::connect(sink, &LegacyModuleSink::renderNeeded,
                   [&]() { pqRenderView->render(); });

  window.show();
  pqRenderView->render();

  return app.exec();
}
