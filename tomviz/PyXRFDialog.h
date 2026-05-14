/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPyXRFDialog_h
#define tomvizPyXRFDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace tomviz {

class PyXRFDialog : public QDialog
{
  Q_OBJECT

public:
  explicit PyXRFDialog(QWidget* parent);
  ~PyXRFDialog() override;

  virtual void show();

  QString command() const;
  QString workingDirectory() const;
  QString scanRange() const;
  QString skipScanIds() const;
  bool skipDownloads() const;
  bool redownloadSuccessful() const;
  QString parametersFile() const;
  QString icName() const;
  bool skipProcessed() const;
  bool rotateDatasets() const;
  QString csvOutput() const;

private:
  class Internal;
  QScopedPointer<Internal> m_internal;
};

} // namespace tomviz

#endif
