/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineExecutionFuture_h
#define tomvizPipelineExecutionFuture_h

#include <QObject>

namespace tomviz {
namespace pipeline {

class Pipeline;

class ExecutionFuture : public QObject
{
  Q_OBJECT

public:
  ExecutionFuture(QObject* parent = nullptr);
  ~ExecutionFuture() override = default;

  bool isFinished() const;
  bool wasCanceled() const;
  bool succeeded() const;

  void deleteWhenFinished();

signals:
  void finished();
  void canceled();

private:
  friend class Pipeline;

  void setFinished(bool success);
  void setCanceled();

  bool m_finished = false;
  bool m_canceled = false;
  bool m_success = false;
};

} // namespace pipeline
} // namespace tomviz

#endif
