/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineVolumeOutputPort_h
#define tomvizPipelineVolumeOutputPort_h

#include "OutputPort.h"

namespace tomviz {
namespace pipeline {

/// An OutputPort that knows how to apply intermediate VolumeData updates
/// on the main thread (deep-copying into the existing vtkImageData).
class VolumeOutputPort : public OutputPort
{
  Q_OBJECT

public:
  VolumeOutputPort(const QString& name, PortType type,
                   QObject* parent = nullptr);

  void setIntermediateData(const PortData& data) override;
};

} // namespace pipeline
} // namespace tomviz

#endif
