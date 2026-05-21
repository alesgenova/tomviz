/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineInputsNotReadyWidget_h
#define tomvizPipelineInputsNotReadyWidget_h

#include <QWidget>

class QPushButton;

namespace tomviz {
namespace pipeline {

/// Placeholder shown in the properties panel / edit dialog when a node
/// has a custom widget that requires live input data, but the inputs
/// aren't yet linked + current. Carries an amber explanatory label and
/// a "Run Pipeline to Generate Inputs" button.
class InputsNotReadyWidget : public QWidget
{
  Q_OBJECT

public:
  explicit InputsNotReadyWidget(QWidget* parent = nullptr);
  ~InputsNotReadyWidget() override = default;

  void setRunEnabled(bool enabled);

signals:
  void runRequested();

private:
  QPushButton* m_runButton = nullptr;
};

} // namespace pipeline
} // namespace tomviz

#endif
