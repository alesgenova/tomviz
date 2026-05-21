/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ProgressReader.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

namespace tomviz {
namespace pipeline {

ProgressReader::ProgressReader(const QString& path, QObject* parent)
  : QObject(parent), m_path(path)
{
}

QString ProgressReader::path() const
{
  return m_path;
}

void ProgressReader::handleMessage(const QString& message)
{
  if (message.isEmpty()) {
    return;
  }

  auto doc = QJsonDocument::fromJson(message.toLatin1());
  if (!doc.isObject()) {
    qCritical()
      << QString("Invalid progress message '%1'").arg(message);
    return;
  }
  auto obj = doc.object();
  auto type = obj.value(QStringLiteral("type")).toString();

  // Per-node messages carry an "operator" field whose value is the node
  // id assigned by the subprocess pipeline. Pipeline-level events omit
  // it.
  if (obj.contains(QStringLiteral("operator"))) {
    int nodeId = obj.value(QStringLiteral("operator")).toInt();

    if (type == QLatin1String("started")) {
      emit nodeStarted(nodeId);
    } else if (type == QLatin1String("finished")) {
      emit nodeFinished(nodeId);
    } else if (type == QLatin1String("error")) {
      emit nodeError(nodeId,
                     obj.value(QStringLiteral("error")).toString());
    } else if (type == QLatin1String("progress.maximum")) {
      emit nodeProgressMaximum(
        nodeId, obj.value(QStringLiteral("value")).toInt());
    } else if (type == QLatin1String("progress.step")) {
      emit nodeProgressStep(
        nodeId, obj.value(QStringLiteral("value")).toInt());
    } else if (type == QLatin1String("progress.message")) {
      emit nodeProgressMessage(
        nodeId, obj.value(QStringLiteral("value")).toString());
    } else if (type == QLatin1String("progress.data")) {
      emit nodeProgressData(
        nodeId, obj.value(QStringLiteral("value")).toString());
    } else {
      qCritical() << QString("Unrecognized message type: %1").arg(type);
    }
    return;
  }

  if (type == QLatin1String("started")) {
    emit pipelineStarted();
  } else if (type == QLatin1String("finished")) {
    emit pipelineFinished();
  } else {
    qCritical() << QString("Unrecognized message type: %1").arg(type);
  }
}

// -- FilesProgressReader -----------------------------------------------------

FilesProgressReader::FilesProgressReader(const QString& path, QObject* parent)
  : ProgressReader(path, parent), m_watcher(new QFileSystemWatcher())
{
  QDir dir(this->path());
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }

  connect(m_watcher.data(), &QFileSystemWatcher::directoryChanged, this,
          &FilesProgressReader::checkForProgressFiles);
}

FilesProgressReader::~FilesProgressReader() = default;

void FilesProgressReader::start()
{
  m_watcher->addPath(this->path());
}

void FilesProgressReader::stop()
{
  m_watcher->removePath(this->path());
}

void FilesProgressReader::sendSignal(const QString& signal)
{
  // Drop a flag file in the progress directory; the subprocess sees
  // it on the next OperatorWrapper getter call. The naming scheme
  // (.flag suffix) keeps it out of the child→parent progress glob
  // checkForProgressFiles uses.
  QString flag = QDir(path()).filePath(signal + QStringLiteral(".flag"));
  QFile f(flag);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "ProgressReader: failed to write signal flag" << flag;
    return;
  }
  f.close();
}

void FilesProgressReader::checkForProgressFiles()
{
  QDir dir(path());
  // Only pick up child→parent progress messages (named progressN by
  // FilesProgress.write); skip our own parent→child *.flag drops so we
  // don't loop on our own writes or treat them as progress JSON.
  const auto files = dir.entryList(
    QStringList{ QStringLiteral("progress*") }, QDir::Files, QDir::Name);
  for (const auto& fileName : files) {
    auto progressFilePath = dir.filePath(fileName);
    QFile progressFile(progressFilePath);
    if (!progressFile.exists()) {
      continue;
    }

    if (!progressFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qCritical() << "Unable to read progress file: " << progressFilePath;
      continue;
    }

    auto msg = QString::fromUtf8(progressFile.readLine());
    progressFile.close();
    if (!msg.isEmpty()) {
      handleMessage(msg);
      progressFile.remove();
    } else {
      // Empty line — file race with the writer. Try again next tick.
      QTimer::singleShot(0, this, &FilesProgressReader::checkForProgressFiles);
    }
  }
}

// -- LocalSocketProgressReader -----------------------------------------------

LocalSocketProgressReader::LocalSocketProgressReader(const QString& path,
                                                     QObject* parent)
  : ProgressReader(path, parent), m_server(new QLocalServer())
{
  connect(m_server.data(), &QLocalServer::newConnection, this, [this]() {
    auto* connection = m_server->nextPendingConnection();
    m_connection.reset(connection);

    if (m_connection) {
      connect(connection, &QIODevice::readyRead, this,
              &LocalSocketProgressReader::readProgress);
      connect(connection, &QLocalSocket::errorOccurred, this,
              [](QLocalSocket::LocalSocketError socketError) {
                if (socketError != QLocalSocket::PeerClosedError) {
                  qCritical()
                    << QString("Socket connection error: %1").arg(socketError);
                }
              });
    }
  });
}

LocalSocketProgressReader::~LocalSocketProgressReader() = default;

void LocalSocketProgressReader::start()
{
  m_server->listen(path());
}

void LocalSocketProgressReader::stop()
{
  m_server->close();
}

void LocalSocketProgressReader::sendSignal(const QString& signal)
{
  if (!m_connection || !m_connection->isOpen()) {
    qWarning() << "ProgressReader: socket not connected; signal" << signal
               << "dropped";
    return;
  }
  // Same JSON-line wire format as child→parent messages but going the
  // other way. The subprocess polls non-blocking on its end of the
  // socket from OperatorWrapper getters.
  QByteArray line =
    QStringLiteral("{\"type\":\"%1\"}\n").arg(signal).toUtf8();
  m_connection->write(line);
  m_connection->flush();
}

void LocalSocketProgressReader::readProgress()
{
  auto message = QString::fromUtf8(m_connection->readLine());
  if (message.isEmpty()) {
    return;
  }

  handleMessage(message);

  // If more data is buffered, schedule ourselves again so we drain it
  // before returning to the event loop.
  if (m_connection->bytesAvailable() > 0) {
    QTimer::singleShot(0, this, &LocalSocketProgressReader::readProgress);
  }
}

} // namespace pipeline
} // namespace tomviz
