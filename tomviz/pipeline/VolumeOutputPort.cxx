/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "VolumeOutputPort.h"

#include "data/VolumeData.h"

#include <vtkImageData.h>
#include <vtkNew.h>

#include <QMetaObject>
#include <QThread>

namespace tomviz {
namespace pipeline {

VolumeOutputPort::VolumeOutputPort(const QString& name, PortType type,
                                   QObject* parent)
  : OutputPort(name, type, parent)
{}

void VolumeOutputPort::setIntermediateData(const PortData& incoming)
{
  auto apply = [this, incoming]() {
    auto fresh = incoming.value<VolumeDataPtr>();
    if (!fresh || !fresh->imageData()) {
      return;
    }
    // Swap a fresh vtkImageData *inside* the existing VolumeData:
    // keeps the VolumeData identity (color map etc.) while giving
    // downstream a new pointer — vtkActiveScalarsProducer caches by
    // pointer and won't refresh on in-place mutation.
    vtkNew<vtkImageData> copy;
    copy->DeepCopy(fresh->imageData());
    auto current = data();
    if (current.isValid() && isVolumeType(current.type())) {
      auto existing = current.value<VolumeDataPtr>();
      if (existing) {
        existing->setImageData(vtkSmartPointer<vtkImageData>(copy.Get()));
        emit dataChanged();
      }
    } else {
      auto vol = std::make_shared<VolumeData>(copy.Get());
      setData(PortData(std::any(vol), type()));
    }

    emit intermediateDataApplied();
  };

  if (QThread::currentThread() == thread()) {
    apply();
  } else {
    QMetaObject::invokeMethod(this, apply, Qt::BlockingQueuedConnection);
  }
}

} // namespace pipeline
} // namespace tomviz
