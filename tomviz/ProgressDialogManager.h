/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizProgressDialogManager_h
#define tomvizProgressDialogManager_h

#include <QObject>
#include <QPointer>

class QDialog;
class QMainWindow;

namespace tomviz {
namespace pipeline {
class Node;
class Pipeline;
} // namespace pipeline

class ProgressDialogManager : public QObject
{
  Q_OBJECT

public:
  ProgressDialogManager(QMainWindow* mw);
  ~ProgressDialogManager() override;

  /// Start tracking execution on @a pipeline.
  void setPipeline(pipeline::Pipeline* pipeline);

protected:
  bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
  void onNodeExecutionStarted(pipeline::Node* node);
  void onNodeExecutionFinished(pipeline::Node* node, bool success);
  void showStatusBarMessage(const QString& message);

private:
  void connectExecutor();

  QMainWindow* m_mainWindow = nullptr;
  QPointer<pipeline::Pipeline> m_pipeline;
  QPointer<QDialog> m_progressDialog;

  Q_DISABLE_COPY(ProgressDialogManager)
};
} // namespace tomviz
#endif
