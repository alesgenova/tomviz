/* Manual demo app for the new pipeline with properties panel.
   Build target: pipelineDemo

   Pipeline:  SphereSource -> InvertData -> AddConstant -> [Outline, Slice, Volume]
   The three sinks are displayed simultaneously in a ParaView render view.
   A left dock shows the PipelineStripWidget on top and the
   VolumePropertiesWidget below (shown only when a Volume port is selected).
*/

#include "EditNodeWidget.h"
#include "Node.h"
#include "OutputPort.h"
#include "Pipeline.h"
#include "PipelineStripWidget.h"
#include "PortType.h"
#include "TransformNode.h"
#include "VolumePropertiesWidget.h"
#include "sources/SphereSource.h"
#include "transforms/LegacyPythonTransform.h"

#include "sinks/ContourSink.h"
#include "sinks/LegacyModuleSink.h"
#include "sinks/OutlineSink.h"
#include "sinks/SliceSink.h"
#include "sinks/VolumeSink.h"

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqServerResource.h>
#include <pqView.h>
#include <vtkSMViewProxy.h>

#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>

using namespace tomviz::pipeline;

namespace {

QString readFile(const QString& path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  return QString::fromUtf8(file.readAll());
}

} // namespace

int main(int argc, char** argv)
{
  // Ensure Python can find the tomviz package (for pipeline_dataset, etc.)
  QByteArray pythonPath = qgetenv("PYTHONPATH");
  QByteArray tomvizPython = TOMVIZ_PYTHON_DIR;
  if (pythonPath.isEmpty()) {
    qputenv("PYTHONPATH", tomvizPython);
  } else {
    qputenv("PYTHONPATH", tomvizPython + ":" + pythonPath);
  }

  QApplication app(argc, argv);

  // ParaView application core
  pqPVApplicationCore pvCore(argc, argv);
  auto* builder = pqApplicationCore::instance()->getObjectBuilder();
  builder->createServer(pqServerResource("builtin:"));
  auto* server = pqApplicationCore::instance()->getActiveServer();

  // Create a 3D render view
  auto* pqRenderView = builder->createView("RenderView", server);
  auto* viewProxy = pqRenderView->getViewProxy();

  // -------------------------------------------------------------------
  // Build the pipeline:
  //   SphereSource -> InvertData -> AddConstant -> [Outline, Slice, Volume]
  // -------------------------------------------------------------------
  auto* pipeline = new Pipeline();

  auto* source = new SphereSource();
  source->setDimensions(64, 64, 64);
  pipeline->addNode(source);

  QString pythonDir = TOMVIZ_PYTHON_DIR;

  // InvertData transform (function-based inline script; the shipped
  // InvertData.py uses a class-based pattern not supported here)
  auto* invert = new LegacyPythonTransform();
  invert->setJSONDescription(readFile(pythonDir + "/InvertData.json"));
  // invert->setJSONDescription(readFile(pythonDir + "/InvertData.py"));
  invert->setScript(
    "def transform(dataset):\n"
    "    import numpy as np\n"
    "    scalars = dataset.active_scalars\n"
    "    if scalars is None:\n"
    "        raise RuntimeError('No scalars found!')\n"
    "    dataset.active_scalars = np.max(scalars) - scalars + np.min(scalars)\n");
  pipeline->addNode(invert);
  pipeline->createLink(source->outputPort("volume"),
                       invert->inputPort("volume"));

  // AddConstant transform
  // auto* addConst = invert;
  auto* addConst = new LegacyPythonTransform();
  addConst->setJSONDescription(readFile(pythonDir + "/AddConstant.json"));
  addConst->setScript(readFile(pythonDir + "/AddConstant.py"));
  addConst->setParameter("constant", -10.0);
  pipeline->addNode(addConst);
  pipeline->createLink(invert->outputPort("volume"),
                       addConst->inputPort("volume"));

  // Create three sinks all connected to the AddConstant output
  auto* outlineSink = new OutlineSink();
  pipeline->addNode(outlineSink);
  pipeline->createLink(addConst->outputPort("volume"),
                       outlineSink->inputPort("volume"));

  auto* sliceSink = new SliceSink();
  pipeline->addNode(sliceSink);
  pipeline->createLink(addConst->outputPort("volume"),
                       sliceSink->inputPort("volume"));

  auto* volumeSink = new VolumeSink();
  pipeline->addNode(volumeSink);
  pipeline->createLink(addConst->outputPort("volume"),
                       volumeSink->inputPort("volume"));

  auto* contourSink = new ContourSink();
  pipeline->addNode(contourSink);
  pipeline->createLink(addConst->outputPort("volume"),
                       contourSink->inputPort("volume"));

  // Execute the source and full pipeline
  source->execute();
  pipeline->execute();

  // Initialize all sinks with the render view
  outlineSink->initialize(viewProxy);
  sliceSink->initialize(viewProxy);
  volumeSink->initialize(viewProxy);
  contourSink->initialize(viewProxy);

  // Re-execute so sinks create VTK actors now that the view is set up
  pipeline->execute();

  // Reset the camera to frame the data
  pqRenderView->resetDisplay();

  // -------------------------------------------------------------------
  // Window layout
  // -------------------------------------------------------------------
  QMainWindow window;
  window.setWindowTitle("Pipeline Demo");
  window.resize(1200, 800);

  // Central area: render view
  auto* central = new QWidget();
  auto* centralLayout = new QVBoxLayout(central);
  centralLayout->setContentsMargins(0, 0, 0, 0);

  // Top toolbar with execute button
  auto* toolbar = new QWidget();
  auto* toolbarLayout = new QHBoxLayout(toolbar);
  toolbarLayout->setContentsMargins(4, 4, 4, 4);

  auto* executeBtn = new QPushButton("Execute");
  toolbarLayout->addWidget(executeBtn);
  toolbarLayout->addStretch();

  centralLayout->addWidget(toolbar);
  centralLayout->addWidget(pqRenderView->widget(), 1);
  window.setCentralWidget(central);

  // Left dock: pipeline strip + properties widget in a vertical splitter
  auto* dock = new QDockWidget("Pipeline", &window);
  dock->setMinimumWidth(240);

  auto* dockContainer = new QWidget();
  auto* dockLayout = new QVBoxLayout(dockContainer);
  dockLayout->setContentsMargins(0, 0, 0, 0);
  dockLayout->setSpacing(0);

  // Top: pipeline strip in a scroll area
  auto* scroll = new QScrollArea();
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* strip = new PipelineStripWidget();
  strip->setPipeline(pipeline);
  scroll->setWidget(strip);
  dockLayout->addWidget(scroll, 1);

  // Bottom: volume properties widget in a scroll area
  auto* propsScroll = new QScrollArea();
  propsScroll->setWidgetResizable(true);
  propsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // Prevent the properties widget from influencing the dock width
  propsScroll->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

  auto* propsWidget = new VolumePropertiesWidget();
  propsWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  propsScroll->setWidget(propsWidget);
  dockLayout->addWidget(propsScroll, 1);

  // Bottom: sink properties widget in a scroll area
  auto* sinkPropsScroll = new QScrollArea();
  sinkPropsScroll->setWidgetResizable(true);
  sinkPropsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  sinkPropsScroll->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  dockLayout->addWidget(sinkPropsScroll, 1);

  // Hide properties panels initially
  propsScroll->hide();
  sinkPropsScroll->hide();

  dock->setWidget(dockContainer);
  window.addDockWidget(Qt::LeftDockWidgetArea, dock);

  // Status bar
  window.statusBar()->showMessage("Ready");

  // -------------------------------------------------------------------
  // Connections
  // -------------------------------------------------------------------

  // Helper: show properties only for Volume-type output ports
  auto showPropsForPort = [&](OutputPort* port) {
    sinkPropsScroll->hide();
    if (port && port->type() == PortType::ImageData) {
      propsWidget->setOutputPort(port);
      propsScroll->show();
    } else {
      propsWidget->setOutputPort(nullptr);
      propsScroll->hide();
    }
  };

  // Helper: show sink or transform properties when a node is selected
  auto showSinkProps = [&](Node* node) {
    propsWidget->setOutputPort(nullptr);
    propsScroll->hide();

    // Try sink node first
    auto* sink = qobject_cast<LegacyModuleSink*>(node);
    if (sink) {
      auto* w = sink->createPropertiesWidget(pipeline, nullptr);
      if (w) {
        sinkPropsScroll->setWidget(w);
        sinkPropsScroll->show();
        return;
      }
    }

    // Try transform node
    auto* transform = qobject_cast<TransformNode*>(node);
    if (transform && transform->hasPropertiesWidget()) {
      auto* w = transform->createPropertiesWidget(pipeline, nullptr);
      if (w) {
        sinkPropsScroll->setWidget(w);
        sinkPropsScroll->show();
        return;
      }
    }

    sinkPropsScroll->hide();
  };

  // When a port is explicitly selected in the strip, show its properties
  QObject::connect(strip, &PipelineStripWidget::portSelected,
                   [&](OutputPort* port) {
                     showPropsForPort(port);
                     if (port) {
                       window.statusBar()->showMessage(
                         QString("Port selected: %1").arg(port->name()));
                     }
                   });

  // When a node is selected (not a port), show sink properties or hide
  QObject::connect(strip, &PipelineStripWidget::nodeSelected, [&](Node* node) {
    if (node) {
      window.statusBar()->showMessage(
        QString("Selected: %1").arg(node->label()));
    }
    showSinkProps(node);
  });

  // Execute button re-runs the pipeline
  QObject::connect(executeBtn, &QPushButton::clicked, [&]() {
    source->execute();
    pipeline->execute();
    pqRenderView->render();
    window.statusBar()->showMessage("Pipeline executed");
  });

  // Render when any sink signals it needs a render
  auto renderSlot = [&]() { pqRenderView->render(); };
  QObject::connect(outlineSink, &LegacyModuleSink::renderNeeded, renderSlot);
  QObject::connect(sliceSink, &LegacyModuleSink::renderNeeded, renderSlot);
  QObject::connect(volumeSink, &LegacyModuleSink::renderNeeded, renderSlot);
  QObject::connect(contourSink, &LegacyModuleSink::renderNeeded, renderSlot);

  // When transform parameters are applied, re-execute the pipeline and render
  auto reexecuteSlot = [&]() {
    pipeline->execute();
    pqRenderView->render();
    window.statusBar()->showMessage("Pipeline re-executed (parameters applied)");
  };
  QObject::connect(invert, &TransformNode::parametersApplied, reexecuteSlot);
  QObject::connect(addConst, &TransformNode::parametersApplied, reexecuteSlot);

  // When volume data is modified via properties widget, re-execute and render
  QObject::connect(propsWidget, &VolumePropertiesWidget::volumeDataModified,
                   [&]() {
                     pipeline->execute();
                     pqRenderView->render();
                   });

  window.show();
  pqRenderView->render();

  return app.exec();
}
