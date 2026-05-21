/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineThreadedExecutor_h
#define tomvizPipelineThreadedExecutor_h

#include "PipelineExecutor.h"

#include <atomic>

class QThread;

namespace tomviz {
namespace pipeline {

class ExecutionWorker;

class ThreadedExecutor : public PipelineExecutor
{
  Q_OBJECT

public:
  ThreadedExecutor(QObject* parent = nullptr);
  ~ThreadedExecutor() override;

  void execute(const QList<Node*>& nodes, Pipeline* pipeline) override;
  void cancel() override;
  bool isRunning() const override;

private:
  void executePending();

  QThread* m_thread = nullptr;
  ExecutionWorker* m_worker = nullptr;
  std::atomic<bool> m_cancelRequested{ false };
  std::atomic<bool> m_running{ false };
  std::atomic<Node*> m_currentNode{ nullptr };
  QMetaObject::Connection m_breakpointConnection;

  // Pending execution request queued while a run was in progress
  QList<Node*> m_pendingNodes;
  Pipeline* m_pendingPipeline = nullptr;
};

} // namespace pipeline
} // namespace tomviz

#endif
