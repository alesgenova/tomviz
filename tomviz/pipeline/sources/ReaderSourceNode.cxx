/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ReaderSourceNode.h"

#include "FileReader.h"
#include "OutputPort.h"
#include "PortData.h"
#include "data/VolumeData.h"

#include <vtkImageData.h>

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

namespace tomviz {
namespace pipeline {

ReaderSourceNode::ReaderSourceNode(QObject* parent) : SourceNode(parent)
{
  addOutput("volume", PortType::ImageData);
  setLabel("Reader Source");
}

void ReaderSourceNode::setFileNames(const QStringList& fileNames)
{
  m_fileNames = fileNames;
  if (!m_fileNames.isEmpty()) {
    setLabel(QFileInfo(m_fileNames.first()).fileName());
  }
}

QStringList ReaderSourceNode::fileNames() const
{
  return m_fileNames;
}

void ReaderSourceNode::setReaderOptions(const QJsonObject& options)
{
  m_readerOptions = options;
}

QJsonObject ReaderSourceNode::readerOptions() const
{
  return m_readerOptions;
}

bool ReaderSourceNode::execute()
{
  setExecState(NodeExecState::Running);

  if (m_fileNames.isEmpty()) {
    qWarning("ReaderSourceNode: no fileNames set");
    setExecState(NodeExecState::Failed);
    return false;
  }

  // Headless read — shared with LoadDataReaction so every format the
  // interactive loader understands (VTI/TIFF/PNG/JPEG/MRC, EMD/H5/FXI/
  // DataExchange/OME-TIFF, Python-registered, and ParaView readers via
  // a serialized descriptor) round-trips here too.
  ReadResult result = readImageData(m_fileNames, m_readerOptions);
  if (!result.imageData) {
    qWarning("ReaderSourceNode: failed to read '%s'",
             qPrintable(m_fileNames.first()));
    setExecState(NodeExecState::Failed);
    return false;
  }

  auto volume = std::make_shared<VolumeData>(result.imageData);
  volume->setLabel(QFileInfo(m_fileNames.first()).baseName());
  if (!result.tiltAngles.isEmpty()) {
    volume->setTiltAngles(result.tiltAngles);
  }
  PortType dataType =
    volume->hasTiltAngles() ? PortType::TiltSeries : PortType::Volume;
  outputPort("volume")->setDeclaredType(dataType);
  setOutputData("volume", PortData(std::any(volume), dataType));

  setExecState(NodeExecState::Idle);
  return true;
}

QJsonObject ReaderSourceNode::serialize() const
{
  auto json = SourceNode::serialize();
  QJsonArray fileNames;
  for (const auto& name : m_fileNames) {
    fileNames.append(name);
  }
  json[QStringLiteral("fileNames")] = fileNames;
  if (!m_readerOptions.isEmpty()) {
    json[QStringLiteral("readerOptions")] = m_readerOptions;
  }
  return json;
}

bool ReaderSourceNode::deserialize(const QJsonObject& json)
{
  if (!SourceNode::deserialize(json)) {
    return false;
  }
  if (json.contains(QStringLiteral("fileNames"))) {
    QStringList fileNames;
    for (const auto& v : json.value(QStringLiteral("fileNames")).toArray()) {
      fileNames.append(v.toString());
    }
    setFileNames(fileNames);
  }
  if (json.contains(QStringLiteral("readerOptions"))) {
    setReaderOptions(
      json.value(QStringLiteral("readerOptions")).toObject());
  }
  return true;
}

} // namespace pipeline
} // namespace tomviz
