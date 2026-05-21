/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ThreadedExecutor.h"

#include "InputPort.h"
#include "InternalNodeExecutor.h"
#include "Link.h"
#include "Node.h"
#include "NodeExecutor.h"
#include "OutputPort.h"
#include "PassthroughOutputPort.h"
#include "Pipeline.h"
#include "PortData.h"

#include <QHash>
#include <QThread>

#include <memory>

namespace tomviz {
namespace pipeline {

namespace {

OutputPort* resolveSourceOutput(OutputPort* output)
{
  while (auto* pt = qobject_cast<PassthroughOutputPort*>(output)) {
    output = pt->source();
    if (!output) {
      return nullptr;
    }
  }
  return output;
}

} // namespace

class ExecutionWorker : public QObject
{
  Q_OBJECT

public:
  ExecutionWorker(std::atomic<bool>& cancelFlag,
                  std::atomic<Node*>& currentNode)
    : m_cancelFlag(cancelFlag), m_currentNode(currentNode)
  {
  }

public slots:
  void run(const QList<Node*>& nodes, Pipeline* pipeline)
  {
    Q_UNUSED(pipeline);

    // Per-plan strong-ref retainer; see DefaultExecutor::execute for the
    // detailed rationale. Local to this slot so it drops as soon as the
    // plan finishes, evicting any transient outputs that no consumer
    // (e.g. a sink) decided to retain.
    QHash<OutputPort*, std::shared_ptr<PortData>> inflight;

    bool breakpointWasHit = false;

    for (auto* node : nodes) {
      if (m_cancelFlag.load()) {
        emit canceled();
        emit executionDone(false);
        return;
      }

      if (node->hasBreakpoint()) {
        // Skip just this node — siblings that don't depend on it can
        // still run. Downstream consumers see anyInputStale() == true
        // and skip themselves on the next iterations.
        emit breakpointHit(node);
        breakpointWasHit = true;
        continue;
      }

      // No "skip Current" filter here — see DefaultExecutor for the
      // rationale. The plan is trusted to contain only nodes that need
      // to run.

      // Skip nodes whose inputs are stale due to upstream failure/cancellation
      if (node->anyInputStale()) {
        continue;
      }

      // Deliver upstream handles to this node's input ports — see
      // DefaultExecutor for the detailed rationale.
      for (auto* input : node->inputPorts()) {
        auto* link = input->link();
        if (!link || !link->from()) {
          continue;
        }
        auto* source = resolveSourceOutput(link->from());
        if (!source) {
          continue;
        }
        auto it = inflight.constFind(source);
        if (it == inflight.constEnd()) {
          if (auto h = source->take()) {
            it = inflight.insert(source, h);
          }
        }
        if (it != inflight.constEnd()) {
          input->setHandle(it.value());
        }
      }

      auto* nx = node->nodeExecutor();
      if (!nx) {
        nx = &InternalNodeExecutor::instance();
      }

      m_currentNode.store(node);
      emit nodeStarted(node);
      bool success = nx->execute(node);
      m_currentNode.store(nullptr);
      emit nodeFinished(node, success);

      for (auto* input : node->inputPorts()) {
        input->clearHandle();
      }

      if (!success) {
        // Mark downstream nodes stale so they are skipped.
        node->markStale();
        continue;
      }

      // No eager take — publications sit on the producer's port until
      // an in-plan consumer's input-delivery loop does the lazy take.
    }

    emit executionDone(!breakpointWasHit);
  }

signals:
  void nodeStarted(Node* node);
  void nodeFinished(Node* node, bool success);
  void executionDone(bool success);
  void breakpointHit(Node* node);
  void canceled();

private:
  std::atomic<bool>& m_cancelFlag;
  std::atomic<Node*>& m_currentNode;
};

ThreadedExecutor::ThreadedExecutor(QObject* parent)
  : PipelineExecutor(parent), m_thread(new QThread(this)),
    m_worker(new ExecutionWorker(m_cancelRequested, m_currentNode))
{
  m_worker->moveToThread(m_thread);

  // Forward worker signals to executor signals (queued connections across
  // thread boundary)
  connect(m_worker, &ExecutionWorker::nodeStarted, this,
          &PipelineExecutor::nodeExecutionStarted);
  connect(m_worker, &ExecutionWorker::nodeFinished, this,
          &PipelineExecutor::nodeExecutionFinished);
  connect(m_worker, &ExecutionWorker::canceled, this,
          &PipelineExecutor::canceled);

  connect(m_worker, &ExecutionWorker::executionDone, this,
          [this](bool success) {
            m_running = false;
            emit executionComplete(success);
            executePending();
          });

  m_thread->start();
}

ThreadedExecutor::~ThreadedExecutor()
{
  m_pendingPipeline = nullptr;
  m_pendingNodes.clear();
  cancel();
  m_thread->quit();
  m_thread->wait();
  delete m_worker;
}

void ThreadedExecutor::execute(const QList<Node*>& nodes, Pipeline* pipeline)
{
  if (m_running) {
    // Store the request and cancel; executePending() will pick it up
    // when the current run finishes.
    m_pendingNodes = nodes;
    m_pendingPipeline = pipeline;
    cancel();
    return;
  }

  // Disconnect previous breakpoint forwarding, if any
  if (m_breakpointConnection) {
    disconnect(m_breakpointConnection);
  }

  // Forward breakpointHit to the pipeline for this execution
  m_breakpointConnection = connect(
    m_worker, &ExecutionWorker::breakpointHit, pipeline,
    [pipeline](Node* node) { emit pipeline->breakpointReached(node); });

  m_cancelRequested = false;
  m_running = true;

  // Use invokeMethod to run the worker's slot on its thread
  QMetaObject::invokeMethod(
    m_worker, [this, nodes, pipeline]() { m_worker->run(nodes, pipeline); },
    Qt::QueuedConnection);
}

void ThreadedExecutor::executePending()
{
  if (m_pendingPipeline) {
    auto nodes = m_pendingNodes;
    auto* pipeline = m_pendingPipeline;
    m_pendingNodes.clear();
    m_pendingPipeline = nullptr;
    execute(nodes, pipeline);
  }
}

void ThreadedExecutor::cancel()
{
  m_cancelRequested = true;
  if (auto* node = m_currentNode.load()) {
    // cancelExecution sets the canceled flag, emits the signal, and
    // notifies the per-node executor (so an external one terminates
    // its subprocess).
    node->cancelExecution();
  }
}

bool ThreadedExecutor::isRunning() const
{
  return m_running;
}

} // namespace pipeline
} // namespace tomviz

#include "ThreadedExecutor.moc"
