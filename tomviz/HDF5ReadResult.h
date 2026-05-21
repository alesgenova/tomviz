/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizHDF5ReadResult_h
#define tomvizHDF5ReadResult_h

#include <QVector>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

namespace tomviz {

struct HDF5ReadResult
{
  vtkSmartPointer<vtkImageData> imageData;
  vtkSmartPointer<vtkImageData> darkData;
  vtkSmartPointer<vtkImageData> whiteData;
  QVector<double> tiltAngles;
  bool isTiltSeries = false;
};

} // namespace tomviz

#endif // tomvizHDF5ReadResult_h
