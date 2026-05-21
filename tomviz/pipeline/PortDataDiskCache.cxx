/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PortDataDiskCache.h"

#include "OutputPort.h"
#include "Pipeline.h"
#include "SourceNode.h"
#include "Tvh5Format.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <exception>

namespace tomviz {
namespace pipeline {

namespace {

constexpr int kCacheNodeId = 1;
const QString kCachePortName = QStringLiteral("data");

} // namespace

bool writePortDataToFile(const PortData& data, const QString& path)
{
  if (!data.isValid() || path.isEmpty()) {
    return false;
  }

  // Tvh5Format throws on payload types it can't serialize (anything
  // not Volume/Table/Molecule — including the std::any<int> payloads
  // some unit tests use). Catching here keeps the deleter that
  // invokes us from propagating the throw past the shared_ptr
  // destruction boundary, where it would terminate the process.
  try {
    Pipeline temp;
    auto* source = new SourceNode();
    auto* port = source->addOutput(kCachePortName, data.type());
    temp.addNode(source);
    temp.setNodeId(source, kCacheNodeId);
    port->setData(data);
    source->markCurrent();
    return Tvh5Format::write(path.toStdString(), &temp);
  } catch (const std::exception& e) {
    qWarning() << "writePortDataToFile: failed for" << path << ":"
               << e.what();
    return false;
  } catch (...) {
    qWarning() << "writePortDataToFile: failed for" << path
               << "(unknown exception)";
    return false;
  }
}

PortData readPortDataFromFile(const QString& path)
{
  if (path.isEmpty()) {
    return PortData();
  }

  try {
    auto state = Tvh5Format::readState(path.toStdString());
  if (state.isEmpty()) {
    return PortData();
  }
  auto pipelineJson = state.value(QStringLiteral("pipeline")).toObject();
  auto nodes = pipelineJson.value(QStringLiteral("nodes")).toArray();
  if (nodes.isEmpty()) {
    return PortData();
  }

  auto nodeEntry = nodes.first().toObject();
  int nodeId = nodeEntry.value(QStringLiteral("id")).toInt(-1);
  auto outputs = nodeEntry.value(QStringLiteral("outputPorts")).toObject();
  if (nodeId < 0 || outputs.isEmpty()) {
    return PortData();
  }
  auto portIt = outputs.constBegin();
  QString portName = portIt.key();
  auto portEntry = portIt.value().toObject();
  PortType type =
    portTypeFromString(portEntry.value(QStringLiteral("type")).toString());
  if (type == PortType::None) {
    return PortData();
  }

  // Rebuild the same minimal shape the writer used so the JSON's
  // node-id references resolve in populatePayloadData.
  Pipeline temp;
  auto* source = new SourceNode();
  auto* port = source->addOutput(portName, type);
  temp.addNode(source);
  temp.setNodeId(source, nodeId);

    Tvh5Format::populatePayloadData(&temp, pipelineJson, path.toStdString());

    // populatePayloadData restores the raw payload bytes but not the
    // port-level metadata (colormap, gradient opacity, scalar renames,
    // …). Apply it explicitly here so the reloaded VolumeData
    // round-trips with the user's customizations intact. The "metadata"
    // sub-object is what OutputPort::serialize() produced when the
    // cache was written.
    auto metadata = portEntry.value(QStringLiteral("metadata")).toObject();
    if (!metadata.isEmpty()) {
      port->deserialize(metadata);
    }

    return port->data();
  } catch (const std::exception& e) {
    qWarning() << "readPortDataFromFile: failed for" << path << ":"
               << e.what();
    return PortData();
  } catch (...) {
    qWarning() << "readPortDataFromFile: failed for" << path
               << "(unknown exception)";
    return PortData();
  }
}

} // namespace pipeline
} // namespace tomviz
