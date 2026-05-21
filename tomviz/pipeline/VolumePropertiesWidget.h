/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineVolumePropertiesWidget_h
#define tomvizPipelineVolumePropertiesWidget_h

#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class QTableWidget;

namespace tomviz {
namespace pipeline {

class OutputPort;
class VolumeData;
class VolumeScalarsModel;

class VolumePropertiesWidget : public QWidget
{
  Q_OBJECT

public:
  explicit VolumePropertiesWidget(QWidget* parent = nullptr);
  ~VolumePropertiesWidget() override;

  void setOutputPort(OutputPort* port);
  OutputPort* outputPort() const;

  bool eventFilter(QObject* obj, QEvent* event) override;

  /// Access the time series label checkbox so callers can wire it to
  /// ActiveObjects::setShowTimeSeriesLabel / showTimeSeriesLabelChanged.
  QCheckBox* showTimeSeriesLabelCheckBox() const
  {
    return m_showTimeSeriesLabel;
  }

  /// Access interaction checkboxes (e.g. for external state queries).
  QCheckBox* translateCheckBox() const { return m_interactTranslate; }
  QCheckBox* rotateCheckBox() const { return m_interactRotate; }
  QCheckBox* scaleCheckBox() const { return m_interactScale; }

  /// Wire up the interactive transform widget to the checkboxes and volume.
  /// Call once after setOutputPort().
  void setupInteractiveTransform();

signals:
  void volumeDataModified();

private:
  void placeTransformWidget();
  void updateTransformWidget();
  void onTransformChanged(const double position[3],
                          const double orientation[3],
                          const double scale[3]);
  void updateData();
  void clear();
  VolumeData* volumeData() const;
  void gatherAndUpdateArraysInfo();
  static QWidget* createSectionHeader(const QString& title, QWidget* parent);
  static QString formatSize(size_t num, bool labelAsBytes = false);

  // Edit slots
  void onLabelEdited();
  void onActiveScalarsChanged(const QString& name);
  void onComponentNameEdited(const QString& name);
  void onUnitsEdited();
  void onLengthEdited(int axis);
  void onVoxelSizeEdited(int axis);
  void onOriginEdited(int axis);
  void onOrientationEdited(int axis);
  void updateTransformFields();
  void onScalarsRenamed(const QString& oldName, const QString& newName);

  // Tilt angles slots
  void onTiltAnglesModified(int row, int column);
  void saveTiltAngles();
  void updateTiltAnglesSection();

  // Time series slots
  void updateTimeSeriesSection();
  void editTimeSeries();

  OutputPort* m_port = nullptr;
  bool m_interacting = false;
  VolumeScalarsModel* m_scalarsModel;
  QList<int> m_scalarIndexes;

  // Widgets — core properties
  QLineEdit* m_labelEdit;
  QComboBox* m_activeScalarsCombo;
  QLabel* m_componentNamesLabel;
  QComboBox* m_componentNamesCombo;
  QLabel* m_dimensionsLabel;
  QLabel* m_voxelsLabel;
  QLabel* m_memoryLabel;
  QTableView* m_scalarsTable;
  QLineEdit* m_lengthBoxes[3];
  QLineEdit* m_voxelSizeBoxes[3];
  QLineEdit* m_originBoxes[3];
  QLineEdit* m_orientationBoxes[3];
  QLineEdit* m_unitBox;

  // Widgets — interaction
  QGroupBox* m_interactionGroup = nullptr;
  QCheckBox* m_interactTranslate = nullptr;
  QCheckBox* m_interactRotate = nullptr;
  QCheckBox* m_interactScale = nullptr;

  // Widgets — tilt angles
  QWidget* m_tiltAnglesHeader = nullptr;
  QTableWidget* m_tiltAnglesTable = nullptr;
  QPushButton* m_saveTiltAnglesButton = nullptr;

  // Widgets — time series
  QWidget* m_timeSeriesHeader = nullptr;
  QWidget* m_timeSeriesGroup = nullptr;
  QCheckBox* m_showTimeSeriesLabel = nullptr;
  QPushButton* m_editTimeSeriesButton = nullptr;
};

} // namespace pipeline
} // namespace tomviz

#endif
