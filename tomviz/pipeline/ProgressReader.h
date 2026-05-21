/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineProgressReader_h
#define tomvizPipelineProgressReader_h

#include <QObject>
#include <QScopedPointer>
#include <QString>

class QFileSystemWatcher;
class QLocalServer;
class QLocalSocket;

namespace tomviz {
namespace pipeline {

/// Parses JSON-line progress messages produced by `tomviz-pipeline`
/// (see tomviz/python/tomviz/pipeline/progress.py for the wire format).
/// Concrete subclasses implement the transport — Unix domain socket on
/// Linux, polled file directory on macOS/Windows. Each message is
/// surfaced as a typed Qt signal carrying the originating node id;
/// callers (typically ExternalNodeExecutor) map ids back to the
/// parent-side Node* and route the value onto its progress setters.
class ProgressReader : public QObject
{
  Q_OBJECT

public:
  ProgressReader(const QString& path, QObject* parent = nullptr);
  ~ProgressReader() override = default;

  virtual void start() = 0;
  virtual void stop() = 0;

  /// Send a control signal to the subprocess. The signal argument is
  /// a short tag like "cancel" or "complete"; subclasses route it
  /// through their transport (a JSON line on the socket, a flag file
  /// in the polled directory, etc.). The subprocess reads these
  /// lazily from its OperatorWrapper getters so we don't add a reader
  /// thread on the child side.
  virtual void sendSignal(const QString& signal) = 0;

  QString path() const;

signals:
  /// Per-node lifecycle events. `nodeId` is the id assigned in the
  /// (shim) pipeline executed by the subprocess.
  void nodeStarted(int nodeId);
  void nodeFinished(int nodeId);
  void nodeError(int nodeId, const QString& error);

  /// Per-node progress updates.
  void nodeProgressMaximum(int nodeId, int maximum);
  void nodeProgressStep(int nodeId, int step);
  void nodeProgressMessage(int nodeId, const QString& message);

  /// Path (relative to the progress directory) of an EMD file written
  /// by the subprocess containing live preview data. Consumer is
  /// responsible for reading the file.
  void nodeProgressData(int nodeId, const QString& dataPath);

  /// Pipeline-level lifecycle events emitted with no node id.
  void pipelineStarted();
  void pipelineFinished();

protected:
  /// Subclasses call this with one complete JSON message at a time.
  void handleMessage(const QString& message);

private:
  QString m_path;
};

class FilesProgressReader : public ProgressReader
{
  Q_OBJECT

public:
  FilesProgressReader(const QString& path, QObject* parent = nullptr);
  ~FilesProgressReader() override;

  void start() override;
  void stop() override;
  void sendSignal(const QString& signal) override;

private:
  void checkForProgressFiles();

  QScopedPointer<QFileSystemWatcher> m_watcher;
};

class LocalSocketProgressReader : public ProgressReader
{
  Q_OBJECT

public:
  LocalSocketProgressReader(const QString& path, QObject* parent = nullptr);
  ~LocalSocketProgressReader() override;

  void start() override;
  void stop() override;
  void sendSignal(const QString& signal) override;

private:
  void readProgress();

  QScopedPointer<QLocalServer> m_server;
  QScopedPointer<QLocalSocket> m_connection;
};

} // namespace pipeline
} // namespace tomviz

#endif
