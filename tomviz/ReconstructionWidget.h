/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizReconstructionWidget_h
#define tomvizReconstructionWidget_h

#include <QWidget>

class vtkImageData;
class vtkSMProxy;

namespace tomviz {

class ReconstructionWidget : public QWidget
{
  Q_OBJECT

public:
  ReconstructionWidget(vtkImageData* inputData, vtkSMProxy* colorMap,
                       QWidget* parent = nullptr);
  ~ReconstructionWidget() override;

public slots:
  void updateProgress(int progress);
  void updateIntermediateResults(std::vector<float> reconSlice);

private:
  Q_DISABLE_COPY(ReconstructionWidget)

  class RWInternal;
  RWInternal* Internals;
};
} // namespace tomviz

#endif
