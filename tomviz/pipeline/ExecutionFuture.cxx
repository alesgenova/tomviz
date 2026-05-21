/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ExecutionFuture.h"

namespace tomviz {
namespace pipeline {

ExecutionFuture::ExecutionFuture(QObject* parent) : QObject(parent) {}

bool ExecutionFuture::isFinished() const
{
  return m_finished;
}

bool ExecutionFuture::wasCanceled() const
{
  return m_canceled;
}

bool ExecutionFuture::succeeded() const
{
  return m_success;
}

void ExecutionFuture::deleteWhenFinished()
{
  if (m_finished) {
    deleteLater();
  } else {
    connect(this, &ExecutionFuture::finished, this,
            &ExecutionFuture::deleteLater);
  }
}

void ExecutionFuture::setFinished(bool success)
{
  if (m_finished) {
    return;
  }
  m_finished = true;
  m_success = success;
  emit finished();
}

void ExecutionFuture::setCanceled()
{
  if (m_finished) {
    return;
  }
  m_canceled = true;
  m_finished = true;
  m_success = false;
  emit canceled();
  emit finished();
}

} // namespace pipeline
} // namespace tomviz
