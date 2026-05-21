/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineVolumeScalarsModel_h
#define tomvizPipelineVolumeScalarsModel_h

#include <QAbstractTableModel>
#include <QList>
#include <QString>

namespace tomviz {
namespace pipeline {

/// Basic scalar array metadata container
struct ScalarArrayInfo
{
  ScalarArrayInfo(QString name, QString range, QString dataType,
                  bool active = false);
  QString name;
  QString range;
  QString dataType;
  bool active;
};

/// Table model displaying scalar array info from vtkPointData.
/// Columns: Active (checkbox), Name (editable), Data Range, Data Type.
class VolumeScalarsModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  explicit VolumeScalarsModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  bool setData(const QModelIndex& index, const QVariant& value,
               int role = Qt::EditRole) override;

  QList<ScalarArrayInfo> arraysInfo() const;

public slots:
  void setArraysInfo(const QList<ScalarArrayInfo>& arraysInfo);

signals:
  void activeScalarsChanged(const QString& name);
  void scalarsRenamed(const QString& oldName, const QString& newName);

private:
  QList<ScalarArrayInfo> m_arraysInfo;
  static const int c_numCol = 4;
  static const int c_activeCol = 0;
  static const int c_nameCol = 1;
  static const int c_rangeCol = 2;
  static const int c_typeCol = 3;
};

} // namespace pipeline
} // namespace tomviz

#endif
