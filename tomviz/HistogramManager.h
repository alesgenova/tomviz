/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizHistogramManager_h

#include <QObject>

#include <vtkSmartPointer.h>

#include <QMap>

class QThread;

class vtkImageData;
class vtkTable;

namespace tomviz {
class HistogramMaker;

class HistogramManager : public QObject
{
  Q_OBJECT

  typedef QObject Superclass;

public:
  static HistogramManager& instance();

  void finalize();

  /// Drop all cached 1D/2D histograms. The caches keep their input
  /// imageData alive (vtkSmartPointer keys) — call this on pipeline
  /// reset to release the memory they're pinning.
  void clearCaches();

  vtkSmartPointer<vtkTable> getHistogram(vtkSmartPointer<vtkImageData> image);
  vtkSmartPointer<vtkImageData> getHistogram2D(
    vtkSmartPointer<vtkImageData> image);

signals:
  void histogramReady(vtkSmartPointer<vtkImageData>, vtkSmartPointer<vtkTable>);
  void histogram2DReady(vtkSmartPointer<vtkImageData> input,
                        vtkSmartPointer<vtkImageData> output);

private slots:
  void histogramReadyInternal(vtkSmartPointer<vtkImageData>,
                              vtkSmartPointer<vtkTable>);
  void histogram2DReadyInternal(vtkSmartPointer<vtkImageData> input,
                                vtkSmartPointer<vtkImageData> output);

private:
  HistogramManager();
  ~HistogramManager();

  // Cache and in-progress lists hold vtkSmartPointer keys so the
  // imageData objects they reference can't be destroyed (and their
  // addresses re-used by an unrelated allocation) while we're still
  // talking about them. Bounded by `kCacheLimit` via LRU eviction so
  // long live-update sessions don't pin arbitrary amounts of memory.
  QMap<vtkSmartPointer<vtkImageData>, vtkSmartPointer<vtkTable>>
    m_histogramCache;
  QMap<vtkSmartPointer<vtkImageData>, vtkSmartPointer<vtkImageData>>
    m_histogram2DCache;
  QList<vtkSmartPointer<vtkImageData>> m_histogramCacheLRU;
  QList<vtkSmartPointer<vtkImageData>> m_histogram2DCacheLRU;
  QList<vtkSmartPointer<vtkImageData>> m_histogramsInProgress;
  QList<vtkSmartPointer<vtkImageData>> m_histogram2DsInProgress;
  HistogramMaker* m_histogramGen;
  QThread* m_worker;
};
} // namespace tomviz

#endif
