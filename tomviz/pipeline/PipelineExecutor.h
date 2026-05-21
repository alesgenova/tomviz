/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePipelineExecutor_h
#define tomvizPipelinePipelineExecutor_h

#include <QList>
#include <QObject>

namespace tomviz {
namespace pipeline {

class Node;
class Pipeline;

class PipelineExecutor : public QObject
{
  Q_OBJECT

public:
  PipelineExecutor(QObject* parent = nullptr);
  ~PipelineExecutor() override = default;

  virtual void execute(const QList<Node*>& nodes, Pipeline* pipeline) = 0;
  virtual void cancel() = 0;
  virtual bool isRunning() const = 0;

signals:
  void nodeExecutionStarted(Node* node);
  void nodeExecutionFinished(Node* node, bool success);
  void executionComplete(bool success);
  void canceled();
};

} // namespace pipeline
} // namespace tomviz

#endif
