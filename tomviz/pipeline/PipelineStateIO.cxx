/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PipelineStateIO.h"

#include "InputPort.h"
#include "Link.h"
#include "Node.h"
#include "NodeExecutorFactory.h"
#include "NodeFactory.h"
#include "OutputPort.h"
#include "Pipeline.h"
#include "SourceNode.h"
#include "sinks/LegacyModuleSink.h"

#include <QDebug>
#include <QJsonArray>

namespace tomviz {
namespace pipeline {

namespace {

constexpr int kSchemaVersion = 2;

} // namespace

bool PipelineStateIO::save(Pipeline* pipeline, QJsonObject& outState)
{
  if (!pipeline) {
    return false;
  }
  NodeFactory::registerBuiltins();
  NodeExecutorFactory::registerBuiltins();

  // Assign ids to every node up front so link references are resolvable.
  for (auto* node : pipeline->nodes()) {
    pipeline->nodeId(node);
  }

  QJsonArray nodesJson;
  for (auto* node : pipeline->nodes()) {
    QJsonObject entry = node->serialize();
    entry[QStringLiteral("id")] = pipeline->nodeId(node);
    entry[QStringLiteral("type")] = NodeFactory::typeName(node);
    nodesJson.append(entry);
  }

  QJsonArray linksJson;
  for (auto* node : pipeline->nodes()) {
    for (auto* input : node->inputPorts()) {
      auto* link = input->link();
      if (!link || !link->from() || !link->to()) {
        continue;
      }
      auto* fromNode = link->from()->node();
      auto* toNode = link->to()->node();
      if (!fromNode || !toNode) {
        continue;
      }
      QJsonObject from;
      from[QStringLiteral("node")] = pipeline->nodeId(fromNode);
      from[QStringLiteral("port")] = link->from()->name();
      QJsonObject to;
      to[QStringLiteral("node")] = pipeline->nodeId(toNode);
      to[QStringLiteral("port")] = link->to()->name();
      QJsonObject entry;
      entry[QStringLiteral("from")] = from;
      entry[QStringLiteral("to")] = to;
      linksJson.append(entry);
    }
  }

  QJsonObject pipelineJson;
  pipelineJson[QStringLiteral("nextNodeId")] = pipeline->nextNodeId();
  pipelineJson[QStringLiteral("nodes")] = nodesJson;
  pipelineJson[QStringLiteral("links")] = linksJson;

  outState[QStringLiteral("schemaVersion")] = kSchemaVersion;
  outState[QStringLiteral("pipeline")] = pipelineJson;
  // Views / layouts / palette are written by the caller via
  // ViewsLayoutsSerializer — they depend on the live ParaView proxy
  // manager, which isn't wired up in unit-test contexts. Keeping that
  // out of this function lets tests round-trip the pipeline graph
  // without initializing pqApplicationCore.
  return true;
}

bool PipelineStateIO::load(Pipeline* pipeline, const QJsonObject& state,
                            const QMap<int, vtkSMViewProxy*>& viewIdMap,
                            const PreExecuteHook& preExecuteHook)
{
  if (!pipeline) {
    return false;
  }
  NodeFactory::registerBuiltins();
  NodeExecutorFactory::registerBuiltins();

  auto pipelineJson = state.value(QStringLiteral("pipeline")).toObject();
  if (pipelineJson.isEmpty()) {
    qWarning() << "PipelineStateIO::load: missing 'pipeline' section";
    return false;
  }

  // Pass 1: instantiate every node and assign it the serialized id.
  // Keep a local id->Node map so links can be resolved even if a node
  // was already renumbered on an earlier load (paranoia — the id
  // allocator on Pipeline is authoritative).
  QHash<int, Node*> idToNode;
  auto nodesJson = pipelineJson.value(QStringLiteral("nodes")).toArray();
  for (const auto& nv : nodesJson) {
    auto entry = nv.toObject();
    auto typeName = entry.value(QStringLiteral("type")).toString();
    int id = entry.value(QStringLiteral("id")).toInt(-1);
    if (typeName.isEmpty() || id < 0) {
      qWarning() << "PipelineStateIO::load: skipping node with missing id/type";
      continue;
    }
    Node* node = NodeFactory::create(typeName);
    if (!node) {
      qWarning() << "PipelineStateIO::load: unknown node type" << typeName
                 << "id" << id;
      continue;
    }
    // For visualization sinks, bind the view BEFORE deserialize runs
    // — the saved "visible" flag and other sink state assume a view
    // is in place. View is looked up by the legacy GlobalID stamped
    // into the JSON at save time. Deserialize itself is deferred to
    // the pipeline's first executionFinished to avoid the "Input port
    // 0 … has 0 connections" render warnings that would fire if
    // setVisibility(true) flipped the VTK actor on before consume()
    // had wired data into the filter.
    if (auto* sink = dynamic_cast<LegacyModuleSink*>(node)) {
      int viewId = entry.value(QStringLiteral("viewId")).toInt(-1);
      if (viewId >= 0) {
        if (auto* view = viewIdMap.value(viewId, nullptr)) {
          sink->initialize(view);
        }
      }
      // Sink actors are visible by default; a render between here and
      // the deferred deserialize below would hit "Input port 0 … has
      // 0 connections" warnings because consume() hasn't wired data
      // into the filter yet. Hide the actor now — the deferred
      // handler flips it back on once the sink is Current.
      sink->setVisibility(false);
      pipeline->addNode(node);
      pipeline->setNodeId(node, id);
      idToNode.insert(id, node);

      // Strip any saved NodeState from the sink JSON: a sink is only
      // legitimately Current after consume() has run *in the current
      // session* (wiring data into the VTK filter). Keeping the saved
      // state=Current would cause Pipeline::execute() to skip the
      // sink on the user's next manual run, leaving vtkOutlineFilter
      // (and similar) with no input and logging "Input port 0 has 0
      // connections" warnings on render.
      QJsonObject sinkJson = entry;
      sinkJson.remove(QStringLiteral("state"));
      QObject::connect(
        pipeline, &Pipeline::executionFinished, sink,
        [sink, sinkJson, pipeline]() {
          const bool targetVis = sinkJson.value("visible").toBool(true);
          const bool sinkIsCurrent = (sink->state() == NodeState::Current);
          if (sinkIsCurrent || !targetVis) {
            sink->deserialize(sinkJson);
            return;
          }
          // Apply everything except visible=true; hook the visibility
          // flip to the next executionFinished that leaves the sink
          // Current (i.e. consume() actually ran).
          QJsonObject later = sinkJson;
          later["visible"] = false;
          sink->deserialize(later);
          auto conn = std::make_shared<QMetaObject::Connection>();
          *conn = QObject::connect(
            pipeline, &Pipeline::executionFinished, sink,
            [sink, targetVis, conn]() {
              if (sink->state() == NodeState::Current) {
                sink->setVisibility(targetVis);
                QObject::disconnect(*conn);
              }
            });
        },
        Qt::SingleShotConnection);
      continue;
    }

    if (!node->deserialize(entry)) {
      qWarning() << "PipelineStateIO::load: deserialize failed for node"
                 << typeName << "id" << id;
      delete node;
      continue;
    }
    pipeline->addNode(node);
    pipeline->setNodeId(node, id);
    idToNode.insert(id, node);
  }

  // Restore nextNodeId so later additions don't collide.
  if (pipelineJson.contains(QStringLiteral("nextNodeId"))) {
    pipeline->setNextNodeId(
      pipelineJson.value(QStringLiteral("nextNodeId")).toInt(1));
  }

  // Pass 2: resolve links.
  auto linksJson = pipelineJson.value(QStringLiteral("links")).toArray();
  for (const auto& lv : linksJson) {
    auto entry = lv.toObject();
    auto from = entry.value(QStringLiteral("from")).toObject();
    auto to = entry.value(QStringLiteral("to")).toObject();
    int fromId = from.value(QStringLiteral("node")).toInt(-1);
    int toId = to.value(QStringLiteral("node")).toInt(-1);
    Node* fromNode = idToNode.value(fromId, nullptr);
    Node* toNode = idToNode.value(toId, nullptr);
    if (!fromNode || !toNode) {
      qWarning() << "PipelineStateIO::load: link references unknown node"
                 << fromId << "->" << toId;
      continue;
    }
    auto* outPort =
      fromNode->outputPort(from.value(QStringLiteral("port")).toString());
    auto* inPort =
      toNode->inputPort(to.value(QStringLiteral("port")).toString());
    if (!outPort || !inPort) {
      qWarning() << "PipelineStateIO::load: link references unknown port"
                 << from.value(QStringLiteral("port")).toString() << "->"
                 << to.value(QStringLiteral("port")).toString();
      continue;
    }
    pipeline->createLink(outPort, inPort);
  }

  // Optional hook — container-format loaders (e.g. Tvh5Format) use
  // this to drop voxel data onto source output ports before the
  // fallback eager-execute pass runs. Anything that has already
  // populated a port's data will be skipped below.
  if (preExecuteHook) {
    preExecuteHook(pipeline, pipelineJson);
  }

  // Pass 2.5: eagerly execute source nodes whose output ports are
  // still empty so their data (file contents for ReaderSourceNode,
  // deterministic volumes for SphereSource, etc.) is present on the
  // output port even when the caller declines auto-execute. Matches
  // LegacyStateLoader, which resolves source data synchronously
  // inside buildSource() via LoadDataReaction::loadData.
  for (auto* node : pipeline->nodes()) {
    if (!dynamic_cast<SourceNode*>(node)) {
      continue;
    }
    bool hasData = false;
    for (auto* port : node->outputPorts()) {
      if (port->hasData()) {
        hasData = true;
        break;
      }
    }
    if (!hasData) {
      node->execute();
    }
  }

  // Pass 2.75: re-apply the saved state for every non-sink node.
  // Pass 2 (createLink) cascades markStale on every downstream node
  // as a side effect, wiping the state Node::deserialize applied in
  // pass 1. Re-apply explicitly so transforms saved as Current go
  // back to Current (they won't re-execute) — provided their output
  // ports really have data, which pass 3 below will verify.
  // setStateNoCascade avoids re-staling downstream: we already know
  // the full graph's intended state, so cascades would just fight us.
  // Sinks are skipped: their saved state is intentionally dropped
  // (consume() has to run in the current session to make them
  // legitimately Current).
  for (const auto& nv : nodesJson) {
    auto entry = nv.toObject();
    int nid = entry.value(QStringLiteral("id")).toInt(-1);
    if (nid < 0) {
      continue;
    }
    auto* n = pipeline->nodeById(nid);
    if (!n || dynamic_cast<LegacyModuleSink*>(n)) {
      continue;
    }
    auto s = entry.value(QStringLiteral("state")).toString();
    if (s == QLatin1String("Current")) {
      n->setStateNoCascade(NodeState::Current);
    } else if (s == QLatin1String("Stale")) {
      n->setStateNoCascade(NodeState::Stale);
    }
    // Missing / "New" — leave at whatever markStale-from-createLink
    // left. A fresh Stale is fine for a node that was never executed.
  }

  // Pass 3: a node was saved as Current because its outputs carried
  // data at save time. Transient ports never serialize their payload,
  // and persistent ports may be empty too (.tvsm, or .tvh5 files where
  // voxels weren't yet reconstructed) — so the restored "Current"
  // state is a lie whenever an output that downstream depends on (or
  // a persistent output that should round-trip its payload) is empty.
  // Downgrade those nodes to Stale; markStale cascades through the
  // links created in pass 2 so downstream nodes also re-execute.
  for (auto* node : pipeline->nodes()) {
    if (node->state() != NodeState::Current) {
      continue;
    }
    for (auto* port : node->outputPorts()) {
      bool needsData = port->isPersistent() || !port->links().isEmpty();
      if (needsData && !port->hasData()) {
        node->markStale();
        break;
      }
    }
  }

  return true;
}

} // namespace pipeline
} // namespace tomviz
