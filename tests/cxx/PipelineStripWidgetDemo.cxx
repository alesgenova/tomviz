/* Manual demo app for PipelineStripWidget. Build target: pipelineStripDemo */

#include "InputPort.h"
#include "Link.h"
#include "Node.h"
#include "OutputPort.h"
#include "Pipeline.h"
#include "PipelineStripWidget.h"
#include "PortData.h"
#include "PortType.h"
#include "SinkGroupNode.h"
#include "SinkNode.h"
#include "SourceNode.h"
#include "TransformNode.h"

#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace tomviz::pipeline;

// Simple transform subclass for demo
class DemoTransform : public TransformNode
{
public:
  DemoTransform(const QString& label,
                const QList<QPair<QString, PortType>>& inputs,
                const QList<QPair<QString, PortType>>& outputs)
    : TransformNode()
  {
    setLabel(label);
    for (auto& [name, type] : inputs) {
      addInput(name, type);
    }
    for (auto& [name, type] : outputs) {
      addOutput(name, type);
    }
  }

  DemoTransform(const QString& label, int numInputs = 1, int numOutputs = 1)
    : TransformNode()
  {
    setLabel(label);
    for (int i = 0; i < numInputs; ++i) {
      QString name = numInputs == 1 ? "in" : QString("in%1").arg(i);
      addInput(name, PortType::ImageData);
    }
    for (int i = 0; i < numOutputs; ++i) {
      QString name;
      PortType type;
      if (numOutputs == 1) {
        name = "out";
        type = PortType::ImageData;
      } else {
        if (i == 0) {
          name = "volume";
          type = PortType::ImageData;
        } else if (i == 1) {
          name = "table";
          type = PortType::Table;
        } else {
          name = QString("out%1").arg(i);
          type = PortType::Molecule;
        }
      }
      addOutput(name, type);
    }
  }

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>&) override
  {
    return {};
  }
};

class DemoSink : public SinkNode
{
public:
  DemoSink(const QString& label,
           const QList<QPair<QString, PortType>>& inputs = {{"in",
                                                             PortType::ImageData}},
           const QString& iconPath = ":/pipeline/pqVolumeData.png")
    : SinkNode(), m_iconPath(iconPath)
  {
    setLabel(label);
    for (auto& [name, type] : inputs) {
      addInput(name, type);
    }
  }

  QIcon icon() const override
  {
    return QIcon(m_iconPath);
  }

  QIcon actionIcon() const override
  {
    // Use check/error as stand-in for eyeball open/closed
    return m_visible
             ? QIcon(QStringLiteral(":/pipeline/check.png"))
             : QIcon(QStringLiteral(":/pipeline/canceled.png"));
  }

  void triggerAction() override
  {
    m_visible = !m_visible;
  }

protected:
  bool consume(const QMap<QString, PortData>&) override { return true; }

private:
  QString m_iconPath;
  bool m_visible = true;
};

// ---------------------------------------------------------------------------
// Pipeline builders
// ---------------------------------------------------------------------------

// Source -> Gaussian -> Threshold -> Contour -> Volume Stats
static Pipeline* buildLinearPipeline()
{
  auto* p = new Pipeline();

  auto* src = new SourceNode();
  src->setLabel("sample.vti");
  src->addOutput("volume", PortType::ImageData);
  src->setOutputData("volume", PortData(std::any(1), PortType::ImageData));
  p->addNode(src);

  auto* gauss = new DemoTransform("Gaussian Filter");
  p->addNode(gauss);
  p->createLink(src->outputPort("volume"), gauss->inputPort("in"));

  auto* thresh = new DemoTransform("Threshold");
  p->addNode(thresh);
  p->createLink(gauss->outputPort("out"), thresh->inputPort("in"));

  auto* contour = new DemoTransform("Contour");
  p->addNode(contour);
  p->createLink(thresh->outputPort("out"), contour->inputPort("in"));

  auto* stats = new DemoSink("Volume Stats");
  p->addNode(stats);
  p->createLink(contour->outputPort("out"), stats->inputPort("in"));

  return p;
}

// Source -> Segment (vol, table, molecules) -> Stats, Plot, MolViewer
static Pipeline* buildMultiOutputPipeline()
{
  auto* p = new Pipeline();

  auto* src = new SourceNode();
  src->setLabel("Load Data");
  src->addOutput("volume", PortType::ImageData);
  src->setOutputData("volume", PortData(std::any(1), PortType::ImageData));
  p->addNode(src);

  auto* seg = new DemoTransform("Segment", 1, 3);
  p->addNode(seg);
  p->createLink(src->outputPort("volume"), seg->inputPort("in"));

  auto* stats = new DemoTransform("Stats");
  p->addNode(stats);
  p->createLink(seg->outputPort("volume"), stats->inputPort("in"));

  auto* plot = new DemoSink("Plot",
                            {{"table", PortType::Table}});
  p->addNode(plot);
  p->createLink(seg->outputPort("table"), plot->inputPort("table"));

  auto* molView = new DemoSink("Molecule Viewer",
                               {{"mol", PortType::Molecule}});
  p->addNode(molView);
  p->createLink(seg->outputPort("out2"), molView->inputPort("mol"));

  return p;
}

// Source A, Source B -> Merge -> Normalize -> Export
static Pipeline* buildFanInPipeline()
{
  auto* p = new Pipeline();

  auto* srcA = new SourceNode();
  srcA->setLabel("Source A");
  srcA->addOutput("volume", PortType::ImageData);
  srcA->setOutputData("volume", PortData(std::any(1), PortType::ImageData));
  p->addNode(srcA);

  auto* srcB = new SourceNode();
  srcB->setLabel("Source B");
  srcB->addOutput("volume", PortType::ImageData);
  srcB->setOutputData("volume", PortData(std::any(2), PortType::ImageData));
  p->addNode(srcB);

  auto* merge = new DemoTransform(
    "Merge",
    {{"volA", PortType::ImageData}, {"volB", PortType::ImageData}},
    {{"merged", PortType::ImageData}});
  p->addNode(merge);
  p->createLink(srcA->outputPort("volume"), merge->inputPort("volA"));
  p->createLink(srcB->outputPort("volume"), merge->inputPort("volB"));

  auto* norm = new DemoTransform("Normalize");
  p->addNode(norm);
  p->createLink(merge->outputPort("merged"), norm->inputPort("in"));

  auto* sink = new DemoSink("Export");
  p->addNode(sink);
  p->createLink(norm->outputPort("out"), sink->inputPort("in"));

  return p;
}

// Source -> Gaussian + Median (fan-out), both -> Compare (fan-in)
static Pipeline* buildFanOutPipeline()
{
  auto* p = new Pipeline();

  auto* src = new SourceNode();
  src->setLabel("sample.vti");
  src->addOutput("volume", PortType::ImageData);
  src->setOutputData("volume", PortData(std::any(1), PortType::ImageData));
  p->addNode(src);

  auto* gauss = new DemoTransform("Gaussian Filter");
  p->addNode(gauss);
  p->createLink(src->outputPort("volume"), gauss->inputPort("in"));

  auto* median = new DemoTransform("Median Filter");
  p->addNode(median);
  p->createLink(src->outputPort("volume"), median->inputPort("in"));

  auto* compare = new DemoTransform(
    "Compare",
    {{"volA", PortType::ImageData}, {"volB", PortType::ImageData}},
    {{"diff", PortType::ImageData}, {"stats", PortType::Table}});
  p->addNode(compare);
  p->createLink(gauss->outputPort("out"), compare->inputPort("volA"));
  p->createLink(median->outputPort("out"), compare->inputPort("volB"));

  auto* view = new DemoSink("Diff Viewer");
  p->addNode(view);
  p->createLink(compare->outputPort("diff"), view->inputPort("in"));

  auto* plot = new DemoSink("Error Plot",
                            {{"table", PortType::Table}});
  p->addNode(plot);
  p->createLink(compare->outputPort("stats"), plot->inputPort("table"));

  return p;
}

// A more complex diamond + multi-output topology:
// Source (TiltSeries) -> Denoise -> Segment (vol, table)
//                               \-> Align -> Reconstruct (->Volume) -> Export
//           Segment.table -> Plot
static Pipeline* buildComplexPipeline()
{
  auto* p = new Pipeline();

  auto* src = new SourceNode();
  src->setLabel("tilt_series.mrc");
  src->addOutput("volume", PortType::TiltSeries);
  src->setOutputData("volume", PortData(std::any(1), PortType::ImageData));
  p->addNode(src);

  // Denoise is generic (ImageData -> ImageData), infers TiltSeries from source
  auto* denoise = new DemoTransform("Denoise (BM3D)");
  p->addNode(denoise);
  p->createLink(src->outputPort("volume"), denoise->inputPort("in"));

  auto* segment = new DemoTransform(
    "Segment",
    {{"in", PortType::ImageData}},
    {{"labels", PortType::ImageData}, {"stats", PortType::Table}});
  p->addNode(segment);
  p->createLink(denoise->outputPort("out"), segment->inputPort("in"));

  // Align takes TiltSeries (inferred through denoise)
  auto* align = new DemoTransform("Align Tilt");
  p->addNode(align);
  p->createLink(denoise->outputPort("out"), align->inputPort("in"));

  // Reconstruct takes ImageData, outputs Volume
  auto* recon = new DemoTransform(
    "Reconstruct (SIRT)",
    {{"in", PortType::ImageData}},
    {{"out", PortType::Volume}});
  p->addNode(recon);
  p->createLink(align->outputPort("out"), recon->inputPort("in"));

  auto* exportSink = new DemoSink("Export Volume");
  p->addNode(exportSink);
  p->createLink(recon->outputPort("out"), exportSink->inputPort("in"));

  auto* plot = new DemoSink("Segmentation Plot",
                            {{"table", PortType::Table}});
  p->addNode(plot);
  p->createLink(segment->outputPort("stats"), plot->inputPort("table"));

  return p;
}

// Source (TiltSeries) -> Reconstruction (TiltSeries->Volume) -> Pad -> Align
//   -> Denoise (vol, table)
//   Denoise.vol -> Outline, Slice, Volume (fan-out to 3 sinks)
//   Denoise.table -> Plot
static Pipeline* buildTomographyPipeline()
{
  auto* p = new Pipeline();

  auto* src = new SourceNode();
  src->setLabel("experiment.tiff");
  src->addOutput("Tilt Series", PortType::TiltSeries);
  src->setOutputData("Tilt Series",
                     PortData(std::any(1), PortType::ImageData));
  p->addNode(src);

  // Reconstruction takes TiltSeries, produces Volume
  auto* recon = new DemoTransform(
    "Reconstruction",
    {{"in", PortType::TiltSeries}},
    {{"Volume", PortType::Volume}});
  p->addNode(recon);
  p->createLink(src->outputPort("Tilt Series"), recon->inputPort("in"));

  // Pad, Align, Denoise are generic Volume->Volume transforms
  auto* pad = new DemoTransform(
    "Pad",
    {{"in", PortType::Volume}},
    {{"Volume", PortType::Volume}});
  p->addNode(pad);
  p->createLink(recon->outputPort("Volume"), pad->inputPort("in"));

  auto* align = new DemoTransform(
    "Align",
    {{"in", PortType::Volume}},
    {{"Volume", PortType::Volume}});
  p->addNode(align);
  p->createLink(pad->outputPort("Volume"), align->inputPort("in"));

  auto* denoise = new DemoTransform(
    "Denoise",
    {{"in", PortType::Volume}},
    {{"Volume", PortType::Volume}, {"Metrics", PortType::Table}});
  p->addNode(denoise);
  p->createLink(align->outputPort("Volume"), denoise->inputPort("in"));

  auto* plot = new DemoSink("Plot",
                            {{"table", PortType::Table}});
  p->addNode(plot);
  p->createLink(denoise->outputPort("Metrics"), plot->inputPort("table"));

  // Visualization sinks accept any volume-like data (ImageData)
  auto* outline = new DemoSink("Outline");
  p->addNode(outline);
  p->createLink(denoise->outputPort("Volume"), outline->inputPort("in"));

  auto* slice = new DemoSink("Slice");
  p->addNode(slice);
  p->createLink(denoise->outputPort("Volume"), slice->inputPort("in"));

  auto* volume = new DemoSink("Volume");
  p->addNode(volume);
  p->createLink(denoise->outputPort("Volume"), volume->inputPort("in"));

  return p;
}

// Left branch:  Source -> Gaussian -> SinkGroup -> Outline, Slice, Volume
// Right branch: Source -> Median  -> Outline2, Slice2, Volume2 (no group)
static Pipeline* buildSinkGroupPipeline()
{
  auto* p = new Pipeline();

  // --- Branch A: Source -> Transform -> SinkGroup -> 3 sinks ---
  auto* srcA = new SourceNode();
  srcA->setLabel("sample_A.vti");
  srcA->addOutput("volume", PortType::ImageData);
  srcA->setOutputData("volume", PortData(std::any(1), PortType::ImageData));
  p->addNode(srcA);

  auto* gauss = new DemoTransform("Gaussian Filter");
  p->addNode(gauss);
  p->createLink(srcA->outputPort("volume"), gauss->inputPort("in"));

  auto* group = new SinkGroupNode();
  group->addPassthrough("volume", PortType::ImageData);
  p->addNode(group);
  p->createLink(gauss->outputPort("out"), group->inputPort("volume"));

  auto* outline = new DemoSink("Outline");
  p->addNode(outline);
  p->createLink(group->outputPort("volume"), outline->inputPort("in"));

  auto* slice = new DemoSink("Slice");
  p->addNode(slice);
  p->createLink(group->outputPort("volume"), slice->inputPort("in"));

  auto* volume = new DemoSink("Volume");
  p->addNode(volume);
  p->createLink(group->outputPort("volume"), volume->inputPort("in"));

  // --- Branch B: Source -> Transform -> 3 sinks (no group) ---
  auto* srcB = new SourceNode();
  srcB->setLabel("sample_B.vti");
  srcB->addOutput("volume", PortType::ImageData);
  srcB->setOutputData("volume", PortData(std::any(1), PortType::ImageData));
  p->addNode(srcB);

  auto* median = new DemoTransform("Median Filter");
  p->addNode(median);
  p->createLink(srcB->outputPort("volume"), median->inputPort("in"));

  auto* outline2 = new DemoSink("Outline 2");
  p->addNode(outline2);
  p->createLink(median->outputPort("out"), outline2->inputPort("in"));

  auto* slice2 = new DemoSink("Slice 2");
  p->addNode(slice2);
  p->createLink(median->outputPort("out"), slice2->inputPort("in"));

  auto* volume2 = new DemoSink("Volume 2");
  p->addNode(volume2);
  p->createLink(median->outputPort("out"), volume2->inputPort("in"));

  return p;
}

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  QMainWindow window;
  window.setWindowTitle("PipelineStripWidget Demo");
  window.resize(800, 600);

  // --- Central widget: pipeline selector + node creation ---
  auto* central = new QWidget();
  auto* centralLayout = new QVBoxLayout(central);

  // Pipeline selector
  auto* combo = new QComboBox();
  combo->addItem("Linear");
  combo->addItem("Multi-Output");
  combo->addItem("Fan-In");
  combo->addItem("Fan-Out");
  combo->addItem("Complex");
  combo->addItem("Tomography");
  combo->addItem("Sink Group");
  combo->addItem("Empty");
  centralLayout->addWidget(combo);

  // Sort order selector
  auto* sortCombo = new QComboBox();
  sortCombo->addItem("Default (Kahn's)");
  sortCombo->addItem("Stable (creation order)");
  sortCombo->addItem("Depth-First (chains)");
  centralLayout->addWidget(sortCombo);

  // --- Helpers ---
  auto makeSpin = [](int defaultVal) {
    auto* spin = new QSpinBox();
    spin->setRange(0, 8);
    spin->setValue(defaultVal);
    return spin;
  };

  // Spinbox group for the 5 port types
  struct PortSpinBoxes
  {
    QSpinBox* imageData = nullptr;
    QSpinBox* tiltSeries = nullptr;
    QSpinBox* volume = nullptr;
    QSpinBox* table = nullptr;
    QSpinBox* molecule = nullptr;
  };

  // Build a port list from five type-count spinners
  auto collectPorts = [](const PortSpinBoxes& s, const QString& prefix)
    -> QList<QPair<QString, PortType>> {
    QList<QPair<QString, PortType>> ports;
    int total = s.imageData->value() + s.tiltSeries->value() +
                s.volume->value() + s.table->value() + s.molecule->value();
    int idx = 0;
    auto add = [&](int count, PortType type, const QString& tag) {
      for (int i = 0; i < count; ++i) {
        QString name = (total == 1 && idx == 0)
                         ? prefix
                         : QString("%1_%2%3").arg(prefix, tag,
                                                  count > 1
                                                    ? QString::number(i)
                                                    : QString());
        ports.append({ name, type });
        ++idx;
      }
    };
    add(s.imageData->value(), PortType::ImageData, "img");
    add(s.tiltSeries->value(), PortType::TiltSeries, "ts");
    add(s.volume->value(), PortType::Volume, "vol");
    add(s.table->value(), PortType::Table, "tbl");
    add(s.molecule->value(), PortType::Molecule, "mol");
    return ports;
  };

  // Build a port-count grid: "         | Input | Output" header + per-type rows
  // Pass showInput=false for Source, showOutput=false for Sink.
  auto makePortGrid = [&makeSpin](QVBoxLayout* parent,
                                  bool showInput, bool showOutput,
                                  int defaultInImg, int defaultOutImg)
    -> QPair<PortSpinBoxes, PortSpinBoxes> {
    // Header row
    auto* header = new QHBoxLayout();
    auto* headerLabel = new QLabel();
    headerLabel->setFixedWidth(70);
    header->addWidget(headerLabel);
    if (showInput) {
      auto* inLabel = new QLabel("Input");
      inLabel->setAlignment(Qt::AlignCenter);
      header->addWidget(inLabel);
    }
    if (showOutput) {
      auto* outLabel = new QLabel("Output");
      outLabel->setAlignment(Qt::AlignCenter);
      header->addWidget(outLabel);
    }
    parent->addLayout(header);

    PortSpinBoxes inputs, outputs;

    auto addRow = [&](const QString& label, int defIn, int defOut)
      -> QPair<QSpinBox*, QSpinBox*> {
      auto* row = new QHBoxLayout();
      auto* lbl = new QLabel(label);
      lbl->setFixedWidth(70);
      row->addWidget(lbl);
      QSpinBox* inSpin = nullptr;
      QSpinBox* outSpin = nullptr;
      if (showInput) {
        inSpin = makeSpin(defIn);
        row->addWidget(inSpin);
      }
      if (showOutput) {
        outSpin = makeSpin(defOut);
        row->addWidget(outSpin);
      }
      parent->addLayout(row);
      return { inSpin, outSpin };
    };

    auto [iImg, oImg] = addRow("ImageData", defaultInImg, defaultOutImg);
    inputs.imageData = iImg; outputs.imageData = oImg;
    auto [iTs, oTs] = addRow("TiltSeries", 0, 0);
    inputs.tiltSeries = iTs; outputs.tiltSeries = oTs;
    auto [iVol, oVol] = addRow("Volume", 0, 0);
    inputs.volume = iVol; outputs.volume = oVol;
    auto [iTbl, oTbl] = addRow("Table", 0, 0);
    inputs.table = iTbl; outputs.table = oTbl;
    auto [iMol, oMol] = addRow("Molecule", 0, 0);
    inputs.molecule = iMol; outputs.molecule = oMol;

    return { inputs, outputs };
  };

  // --- Add Source ---
  auto* srcGroup = new QGroupBox("Add Source");
  auto* srcLayout = new QVBoxLayout(srcGroup);
  auto* srcLabelEdit = new QLineEdit("New Source");
  srcLayout->addWidget(srcLabelEdit);
  auto [srcIn, srcOut] = makePortGrid(srcLayout, false, true, 0, 1);
  auto* srcButton = new QPushButton("Add Source");
  srcLayout->addWidget(srcButton);
  centralLayout->addWidget(srcGroup);

  // --- Add Transform ---
  auto* xfGroup = new QGroupBox("Add Transform");
  auto* xfLayout = new QVBoxLayout(xfGroup);
  auto* xfLabelEdit = new QLineEdit("New Transform");
  xfLayout->addWidget(xfLabelEdit);
  auto [xfIn, xfOut] = makePortGrid(xfLayout, true, true, 1, 1);
  auto* xfButton = new QPushButton("Add Transform");
  xfLayout->addWidget(xfButton);
  centralLayout->addWidget(xfGroup);

  // --- Add Sink ---
  auto* sinkGroup = new QGroupBox("Add Sink");
  auto* sinkLayout = new QVBoxLayout(sinkGroup);
  auto* sinkLabelEdit = new QLineEdit("New Sink");
  sinkLayout->addWidget(sinkLabelEdit);
  auto [sinkIn, sinkOut] = makePortGrid(sinkLayout, true, false, 1, 0);
  auto* sinkButton = new QPushButton("Add Sink");
  sinkLayout->addWidget(sinkButton);
  centralLayout->addWidget(sinkGroup);

  centralLayout->addStretch();
  window.setCentralWidget(central);

  // --- Dock with strip widget ---
  auto* dock = new QDockWidget("Pipeline", &window);
  dock->setMinimumWidth(200);

  auto* scroll = new QScrollArea();
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* strip = new PipelineStripWidget();
  scroll->setWidget(strip);
  dock->setWidget(scroll);
  window.addDockWidget(Qt::LeftDockWidgetArea, dock);

  // Pipeline instances
  Pipeline* pipelines[8] = {  buildLinearPipeline(), buildMultiOutputPipeline(),
                              buildFanInPipeline(), buildFanOutPipeline(),
                              buildComplexPipeline(), buildTomographyPipeline(),
                              buildSinkGroupPipeline(), new Pipeline(),
                            };

  strip->setPipeline(pipelines[0]);

  // --- Add node handlers ---

  QObject::connect(srcButton, &QPushButton::clicked, [&]() {
    auto* p = strip->pipeline();
    if (!p) return;
    QString label = srcLabelEdit->text();
    if (label.isEmpty()) label = "Source";
    auto outputs = collectPorts(srcOut, "out");
    auto* src = new SourceNode();
    src->setLabel(label);
    for (auto& [name, type] : outputs) {
      src->addOutput(name, type);
    }
    p->addNode(src);
  });

  QObject::connect(xfButton, &QPushButton::clicked, [&]() {
    auto* p = strip->pipeline();
    if (!p) return;
    QString label = xfLabelEdit->text();
    if (label.isEmpty()) label = "Transform";
    auto inputs = collectPorts(xfIn, "in");
    auto outputs = collectPorts(xfOut, "out");
    auto* xform = new DemoTransform(label, inputs, outputs);
    p->addNode(xform);
  });

  QObject::connect(sinkButton, &QPushButton::clicked, [&]() {
    auto* p = strip->pipeline();
    if (!p) return;
    QString label = sinkLabelEdit->text();
    if (label.isEmpty()) label = "Sink";
    auto inputs = collectPorts(sinkIn, "in");
    auto* sink = new DemoSink(label, inputs);
    p->addNode(sink);
  });

  // Link validator: compatible type, different nodes, target not already connected
  auto linkValidator = [](OutputPort* from, InputPort* to) -> bool {
    if (from->node() == to->node()) {
      return false;
    }
    if (!isPortTypeCompatible(from->type(), to->acceptedTypes())) {
      return false;
    }
    if (!from->canAcceptLink(to)) {
      return false;
    }
    if (to->link()) {
      // Allow dragging from a SinkGroupNode "+" to a sink that shares
      // the same upstream port as the group.
      auto* sg = qobject_cast<SinkGroupNode*>(from->node());
      if (sg) {
        int idx = sg->outputPorts().indexOf(from);
        if (idx >= 0 && idx < sg->inputPorts().size()) {
          auto* gi = sg->inputPorts()[idx];
          if (gi->link() && gi->link()->from() == to->link()->from()) {
            return true;
          }
        }
      }
      return false;
    }
    return true;
  };
  strip->setLinkValidator(linkValidator);

  // Create the link when requested
  QObject::connect(strip, &PipelineStripWidget::linkRequested,
                   [&](OutputPort* from, InputPort* to) {
                     if (auto* p = strip->pipeline()) {
                       // Remove existing link if stealing a sink into a group.
                       if (to->link()) {
                         p->removeLink(to->link());
                       }
                       p->createLink(from, to);
                       qDebug("Created link: %s.%s -> %s.%s",
                              qPrintable(from->node()->label()),
                              qPrintable(from->name()),
                              qPrintable(to->node()->label()),
                              qPrintable(to->name()));
                     }
                   });

  // Leave-group: relink the member to the group's upstream port
  QObject::connect(strip, &PipelineStripWidget::leaveGroupRequested,
                   [](Node* member, SinkGroupNode* group) {
                     auto* p = qobject_cast<Pipeline*>(group->parent());
                     if (!p) return;
                     for (auto* inPort : member->inputPorts()) {
                       if (!inPort->link()) continue;
                       auto* groupPort = inPort->link()->from();
                       if (groupPort->node() != group) continue;
                       int idx = group->outputPorts().indexOf(groupPort);
                       OutputPort* upstream = nullptr;
                       if (idx >= 0 && idx < group->inputPorts().size()) {
                         auto* gi = group->inputPorts()[idx];
                         if (gi->link()) upstream = gi->link()->from();
                       }
                       p->removeLink(inPort->link());
                       if (upstream) p->createLink(upstream, inPort);
                       break;
                     }
                     qDebug("Left group: %s", qPrintable(member->label()));
                   });

  // Context menu on nodes
  strip->setNodeMenuProvider([](Node* node, QMenu& menu) {
    auto* p = qobject_cast<Pipeline*>(node->parent());
    if (!p) return;

    // SinkNode not in a group: "Create Group"
    auto* sink = qobject_cast<SinkNode*>(node);
    bool inGroup = false;
    if (sink) {
      for (auto* inPort : sink->inputPorts()) {
        if (inPort->link() &&
            qobject_cast<SinkGroupNode*>(inPort->link()->from()->node())) {
          inGroup = true;
          break;
        }
      }
    }
    if (sink && !inGroup) {
      menu.addAction("Create Group", [p, sink]() {
        auto* group = new SinkGroupNode();
        for (auto* inPort : sink->inputPorts()) {
          PortType pt = PortType::ImageData;
          for (auto t : { PortType::ImageData, PortType::TiltSeries,
                          PortType::Volume, PortType::Image,
                          PortType::Table, PortType::Molecule }) {
            if (inPort->acceptedTypes().testFlag(t)) { pt = t; break; }
          }
          group->addPassthrough(inPort->name(), pt);
        }
        p->addNode(group);
        for (int i = 0; i < sink->inputPorts().size(); ++i) {
          auto* sinkInput = sink->inputPorts()[i];
          if (sinkInput->link() && i < group->inputPorts().size()) {
            auto* upstream = sinkInput->link()->from();
            p->removeLink(sinkInput->link());
            p->createLink(upstream, group->inputPorts()[i]);
          }
          if (i < group->outputPorts().size())
            p->createLink(group->outputPorts()[i], sinkInput);
        }
      });
    }

    // Node inside a group: "Leave Group"
    for (auto* inPort : node->inputPorts()) {
      if (!inPort->link()) continue;
      auto* group = qobject_cast<SinkGroupNode*>(
        inPort->link()->from()->node());
      if (!group) continue;
      menu.addAction("Leave Group", [p, node, group]() {
        for (auto* inp : node->inputPorts()) {
          if (!inp->link()) continue;
          auto* groupPort = inp->link()->from();
          if (groupPort->node() != group) continue;
          int idx = group->outputPorts().indexOf(groupPort);
          OutputPort* upstream = nullptr;
          if (idx >= 0 && idx < group->inputPorts().size()) {
            auto* gi = group->inputPorts()[idx];
            if (gi->link()) upstream = gi->link()->from();
          }
          p->removeLink(inp->link());
          if (upstream) p->createLink(upstream, inp);
          break;
        }
      });
      break;
    }

    menu.addAction("Delete", [p, node]() { p->removeNode(node); });
  });

  // Context menu on links: delete action
  strip->setLinkMenuProvider([](Link* link, QMenu& menu) {
    menu.addAction("Delete Link", [link]() {
      auto* p = qobject_cast<Pipeline*>(link->parent());
      if (p) {
        qDebug("Deleting link: %s.%s -> %s.%s",
               qPrintable(link->from()->node()->label()),
               qPrintable(link->from()->name()),
               qPrintable(link->to()->node()->label()),
               qPrintable(link->to()->name()));
        p->removeLink(link);
      }
    });
  });

  QObject::connect(combo, &QComboBox::currentIndexChanged,
                   [&](int idx) { strip->setPipeline(pipelines[idx]); });

  QObject::connect(sortCombo, &QComboBox::currentIndexChanged,
                   [&](int idx) {
                     strip->setSortOrder(static_cast<SortOrder>(idx));
                   });

  QObject::connect(strip, &PipelineStripWidget::nodeSelected,
                   [](Node* node) {
                     if (node)
                       qDebug("Selected node: %s",
                              qPrintable(node->label()));
                   });

  QObject::connect(strip, &PipelineStripWidget::nodeDoubleClicked,
                   [](Node* node) {
                     qDebug("Double-clicked: %s", qPrintable(node->label()));
                   });

  window.show();
  return app.exec();
}
