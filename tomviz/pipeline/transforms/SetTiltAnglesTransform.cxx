/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SetTiltAnglesTransform.h"

#include "EditTransformWidget.h"
#include "InputPort.h"
#include "data/VolumeData.h"

#include <vtkDataArray.h>
#include <vtkFieldData.h>
#include <vtkImageData.h>
#include <vtkNew.h>

#include <QClipboard>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QFileDialog>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cmath>

namespace {

class SetTiltAnglesWidget : public tomviz::pipeline::EditTransformWidget
{
  Q_OBJECT
public:
  SetTiltAnglesWidget(tomviz::pipeline::SetTiltAnglesTransform* op,
                      tomviz::pipeline::VolumeDataPtr volumeData, QWidget* p)
    : EditTransformWidget(p), m_op(op)
  {
    QMap<size_t, double> tiltAngles = m_op->tiltAnglesMap();
    QHBoxLayout* baseLayout = new QHBoxLayout;
    this->setLayout(baseLayout);
    m_tabWidget = new QTabWidget;
    baseLayout->addWidget(m_tabWidget);

    QWidget* setAutomaticPanel = new QWidget;

    QGridLayout* layout = new QGridLayout;

    QString descriptionString =
      "A tomographic \"tilt series\" is a set of projection images taken while "
      "rotating (\"tilting\") the specimen. Setting the correct angles is "
      "needed for accurate reconstruction. Set a linearly spaced range of "
      "angles by specifying the start and end tilt index and start and end "
      "angles.  The tilt angles can also be set in the \"Data Properties\" "
      "panel or from Python.";

    auto* imageData = volumeData->imageData();
    int extent[6];
    imageData->GetExtent(extent);
    int totalSlices = extent[5] - extent[4] + 1;
    m_previousTiltAngles.resize(totalSlices);
    if (totalSlices < 60) {
      m_angleIncrement = 3.0;
    } else if (totalSlices < 80) {
      m_angleIncrement = 2.0;
    } else if (totalSlices < 120) {
      m_angleIncrement = 1.5;
    } else {
      m_angleIncrement = 1.0;
    }

    double startAngleValue = -(totalSlices - 1) * m_angleIncrement / 2.0;
    double endAngleValue =
      startAngleValue + (totalSlices - 1) * m_angleIncrement;

    if (tiltAngles.contains(0) && tiltAngles.contains(totalSlices - 1)) {
      startAngleValue = tiltAngles[0];
      endAngleValue = tiltAngles[totalSlices - 1];
    }

    QLabel* descriptionLabel = new QLabel(descriptionString);
    descriptionLabel->setMinimumHeight(120);
    descriptionLabel->setSizePolicy(QSizePolicy::MinimumExpanding,
                                    QSizePolicy::MinimumExpanding);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel, 0, 0, 1, 4, Qt::AlignCenter);
    layout->addWidget(new QLabel("Start Image #: "), 1, 0, 1, 1,
                      Qt::AlignCenter);
    m_startTilt = new QSpinBox;
    m_startTilt->setRange(0, totalSlices - 1);
    m_startTilt->setValue(0);
    layout->addWidget(m_startTilt, 1, 1, 1, 1, Qt::AlignCenter);
    layout->addWidget(new QLabel("End Image #: "), 2, 0, 1, 1,
                      Qt::AlignCenter);
    m_endTilt = new QSpinBox;
    m_endTilt->setRange(0, totalSlices - 1);
    m_endTilt->setValue(totalSlices - 1);
    layout->addWidget(m_endTilt, 2, 1, 1, 1, Qt::AlignCenter);
    layout->addWidget(new QLabel("Set Start Angle: "), 1, 2, 1, 1,
                      Qt::AlignCenter);
    m_startAngle = new QDoubleSpinBox;
    m_startAngle->setRange(-360.0, 360.0);
    m_startAngle->setValue(startAngleValue);
    layout->addWidget(m_startAngle, 1, 3, 1, 1, Qt::AlignCenter);
    layout->addWidget(new QLabel("Set End Angle: "), 2, 2, 1, 1,
                      Qt::AlignCenter);
    m_endAngle = new QDoubleSpinBox;
    m_endAngle->setRange(-360.0, 360.0);
    m_endAngle->setValue(endAngleValue);
    layout->addWidget(m_endAngle, 2, 3, 1, 1, Qt::AlignCenter);

    layout->addWidget(new QLabel("Angle Increment: "), 3, 2, 1, 1,
                      Qt::AlignCenter);

    QString s = QString::number(m_angleIncrement, 'f', 2);
    m_angleIncrementLabel = new QLabel(s);
    connect(m_startTilt, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &SetTiltAnglesWidget::updateAngleIncrement);
    connect(m_endTilt, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &SetTiltAnglesWidget::updateAngleIncrement);
    connect(m_startAngle,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &SetTiltAnglesWidget::updateAngleIncrement);
    connect(m_endAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SetTiltAnglesWidget::updateAngleIncrement);
    layout->addWidget(m_angleIncrementLabel, 3, 3, 1, 1, Qt::AlignCenter);

    auto outerLayout = new QVBoxLayout;
    outerLayout->addLayout(layout);
    outerLayout->addStretch();

    setAutomaticPanel->setLayout(outerLayout);

    QWidget* setFromTablePanel = new QWidget;
    QVBoxLayout* tablePanelLayout = new QVBoxLayout;
    m_tableWidget = new QTableWidget;
    m_tableWidget->setRowCount(totalSlices);
    m_tableWidget->setColumnCount(1);
    m_tableWidget->setHorizontalHeaderLabels({ "Tilt Angle" });
    tablePanelLayout->addWidget(m_tableWidget);

    QWidget* buttonWidget = new QWidget;
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonWidget->setLayout(buttonLayout);
    tablePanelLayout->addWidget(buttonWidget);

    QPushButton* loadFromFileButton = new QPushButton;
    loadFromFileButton->setText("Load From Text File");
    buttonLayout->addWidget(loadFromFileButton);
    buttonLayout->insertStretch(-1);
    connect(loadFromFileButton, &QPushButton::clicked, this,
            &SetTiltAnglesWidget::loadFromFile);

    // Populate the table with existing tilt angles
    vtkFieldData* fd = imageData->GetFieldData();
    vtkDataArray* tiltArray = fd ? fd->GetArray("tilt_angles") : nullptr;
    // Also check the VolumeData tilt angles
    QVector<double> volTiltAngles = volumeData->tiltAngles();

    for (int i = 0; i < totalSlices; ++i) {
      QTableWidgetItem* item = new QTableWidgetItem;
      double angle = 0;
      if (tiltArray && i < tiltArray->GetNumberOfTuples()) {
        angle = tiltArray->GetTuple1(i);
        m_previousTiltAngles[i] = angle;
      } else if (i < volTiltAngles.size()) {
        angle = volTiltAngles[i];
        m_previousTiltAngles[i] = angle;
      } else {
        m_previousTiltAngles[i] = 0;
      }
      if (tiltAngles.contains(i)) {
        angle = tiltAngles[i];
      }
      item->setData(Qt::DisplayRole, QString::number(angle));
      m_tableWidget->setItem(i, 0, item);
    }
    m_tableWidget->installEventFilter(this);

    setFromTablePanel->setLayout(tablePanelLayout);

    m_tabWidget->addTab(setAutomaticPanel, "Set by Range");
    m_tabWidget->addTab(setFromTablePanel, "Set Individually");

    baseLayout->setSizeConstraint(QLayout::SetMinimumSize);
  }

  void applyChangesToOperator() override
  {
    if (m_tabWidget->currentIndex() == 0) {
      QMap<size_t, double> tiltAngles = m_op->tiltAnglesMap();
      int start = m_startTilt->value();
      int end = m_endTilt->value();
      if (end == start) {
        tiltAngles[start] = m_startAngle->value();
      } else {
        double delta =
          (m_endAngle->value() - m_startAngle->value()) / (end - start);
        double baseAngle = m_startAngle->value();
        if (end < start) {
          int temp = start;
          start = end;
          end = temp;
          baseAngle = m_endAngle->value();
        }
        for (int i = 0; start + i <= end; ++i) {
          tiltAngles.insert(start + i, baseAngle + delta * i);
          m_tableWidget->item(i, 0)->setData(
            Qt::DisplayRole, QString::number(tiltAngles[start + i]));
        }
      }
      m_op->setTiltAngles(tiltAngles);
    } else {
      QMap<size_t, double> tiltAngles;
      for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
        QTableWidgetItem* item = m_tableWidget->item(i, angleColumn());
        tiltAngles[i] = item->data(Qt::DisplayRole).toDouble();
      }
      m_op->setTiltAngles(tiltAngles);

      if (m_hasScanIDs) {
        QVector<int> scanIds;
        scanIds.reserve(m_tableWidget->rowCount());
        for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
          QTableWidgetItem* item = m_tableWidget->item(i, 0);
          scanIds.push_back(item->data(Qt::DisplayRole).toInt());
        }
        m_op->setScanIds(scanIds);
      }
    }
  }

  bool eventFilter(QObject* obj, QEvent* event) override
  {
    QKeyEvent* ke = dynamic_cast<QKeyEvent*>(event);
    if (ke && obj == m_tableWidget) {
      if (ke->matches(QKeySequence::Paste) && ke->type() == QEvent::KeyPress) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        const QMimeData* mimeData = clipboard->mimeData();
        if (mimeData->hasText()) {
          QString text = mimeData->text().trimmed();
          QStringList rows = text.split("\n");
          QStringList angles;
          for (const QString& row : rows) {
            angles << row.split("\t")[0];
          }
          auto ranges = m_tableWidget->selectedRanges();
          for (const QString& angle : angles) {
            bool ok;
            angle.toDouble(&ok);
            if (!ok) {
              QMessageBox::warning(
                this, "Error",
                QString("Error: pasted tilt angle %1 is not a number")
                  .arg(angle));
              return true;
            }
          }
          if (ranges.size() != 1) {
            QMessageBox::warning(
              this, "Error",
              "Pasting is not supported with non-continuous selections");
            return true;
          }
          if (ranges[0].rowCount() > 1 &&
              ranges[0].rowCount() != angles.size()) {
            QMessageBox::warning(
              this, "Error",
              QString("Cells selected (%1) does not match "
                      "number of angles to paste (%2).  \n"
                      "Please select one cell to mark the "
                      "start location for pasting or select "
                      "the same number of cells that will "
                      "be pasted into.")
                .arg(ranges[0].rowCount())
                .arg(angles.size()));
            return true;
          }
          int startRow = ranges[0].topRow();
          for (int i = 0; i < angles.size(); ++i) {
            auto item = m_tableWidget->item(i + startRow, angleColumn());
            if (item) {
              item->setData(Qt::DisplayRole, angles[i]);
            }
          }
        }
        return true;
      }
    }
    return QWidget::eventFilter(obj, event);
  }

public slots:
  void updateAngleIncrement()
  {
    m_angleIncrement = (m_endAngle->value() - m_startAngle->value()) /
                       (m_endTilt->value() - m_startTilt->value());
    QString s;
    if (std::isfinite(m_angleIncrement)) {
      s = QString::number(m_angleIncrement, 'f', 2);
    } else if (m_endAngle->value() == m_startAngle->value()) {
      s = QString::number(0, 'f', 2);
    } else {
      s = "Invalid inputs!";
    }
    m_angleIncrementLabel->setText(s);
  }

  void loadFromFile()
  {
    QStringList filters;
    filters << "Any (*)"
            << "Text (*.txt)"
            << "CSV (*.csv)";

    QFileDialog dialog;
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setObjectName("SetTiltAnglesTransform-loadFromFile");
    dialog.setAcceptMode(QFileDialog::AcceptOpen);

    if (dialog.exec() == QDialog::Accepted) {
      QString content;
      QFile tiltAnglesFile(dialog.selectedFiles()[0]);
      if (tiltAnglesFile.open(QIODevice::ReadOnly)) {
        content = tiltAnglesFile.readAll();
      } else {
        qCritical()
          << QString("Unable to read '%1'.").arg(dialog.selectedFiles()[0]);
        return;
      }

      QStringList rawLines = content.split("\n");
      QList<QStringList> parsedLines;
      for (const QString& rawLine : rawLines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) {
          continue;
        }
        QStringList tokens = line.split(QRegularExpression("\\s+"));
        parsedLines.append(tokens);
      }

      if (parsedLines.isEmpty()) {
        return;
      }

      bool twoColumn = true;
      for (const QStringList& tokens : parsedLines) {
        if (tokens.size() != 2) {
          twoColumn = false;
          break;
        }
        bool ok1, ok2;
        tokens[0].toInt(&ok1);
        tokens[1].toDouble(&ok2);
        if (!ok1 || !ok2) {
          twoColumn = false;
          break;
        }
      }

      int maxRows = std::min(static_cast<int>(parsedLines.size()),
                             m_tableWidget->rowCount());

      if (twoColumn) {
        m_hasScanIDs = true;
        m_tableWidget->setColumnCount(2);
        m_tableWidget->setHorizontalHeaderLabels({ "Scan ID", "Tilt Angle" });
        m_tableWidget->horizontalHeader()->setStretchLastSection(true);

        for (int i = 0; i < maxRows; ++i) {
          QTableWidgetItem* scanItem = new QTableWidgetItem;
          scanItem->setData(Qt::DisplayRole, parsedLines[i][0]);
          scanItem->setFlags(scanItem->flags() & ~Qt::ItemIsEditable);
          m_tableWidget->setItem(i, 0, scanItem);

          QTableWidgetItem* angleItem = new QTableWidgetItem;
          angleItem->setData(Qt::DisplayRole, parsedLines[i][1]);
          m_tableWidget->setItem(i, 1, angleItem);
        }
      } else {
        m_hasScanIDs = false;
        m_tableWidget->setColumnCount(1);
        m_tableWidget->setHorizontalHeaderLabels({ "Tilt Angle" });

        for (int i = 0; i < maxRows; ++i) {
          QTableWidgetItem* item = m_tableWidget->item(i, 0);
          item->setData(Qt::DisplayRole, parsedLines[i][0]);
        }
      }
    }
  }

private:
  QSpinBox* m_startTilt;
  QSpinBox* m_endTilt;
  QDoubleSpinBox* m_startAngle;
  QDoubleSpinBox* m_endAngle;
  QTableWidget* m_tableWidget;
  QTabWidget* m_tabWidget;
  QLabel* m_angleIncrementLabel;
  double m_angleIncrement = 1.0;
  bool m_hasScanIDs = false;

  int angleColumn() const { return m_hasScanIDs ? 1 : 0; }

  QPointer<tomviz::pipeline::SetTiltAnglesTransform> m_op;
  QVector<double> m_previousTiltAngles;
};

} // namespace

#include "SetTiltAnglesTransform.moc"

namespace tomviz {
namespace pipeline {

SetTiltAnglesTransform::SetTiltAnglesTransform(QObject* parent)
  : TransformNode(parent)
{
  addInput("volume", PortType::ImageData);
  addOutput("output", PortType::TiltSeries);
  setLabel("Set Tilt Angles");
}

void SetTiltAnglesTransform::setTiltAngles(
  const QMap<size_t, double>& angles)
{
  m_tiltAngles = angles;
}

QMap<size_t, double> SetTiltAnglesTransform::tiltAnglesMap() const
{
  return m_tiltAngles;
}

void SetTiltAnglesTransform::setScanIds(const QVector<int>& ids)
{
  m_scanIds = ids;
}

QVector<int> SetTiltAnglesTransform::scanIds() const
{
  return m_scanIds;
}

bool SetTiltAnglesTransform::hasPropertiesWidget() const
{
  return true;
}

bool SetTiltAnglesTransform::propertiesWidgetNeedsInput() const
{
  return true;
}

EditTransformWidget* SetTiltAnglesTransform::createPropertiesWidget(
  QWidget* parent)
{
  auto* inputPort = this->inputPorts()[0];
  if (!inputPort || !inputPort->hasData()) {
    return nullptr;
  }

  VolumeDataPtr vol;
  try {
    vol = inputPort->data().value<VolumeDataPtr>();
  } catch (const std::bad_any_cast&) {
    return nullptr;
  }

  if (!vol || !vol->isValid()) {
    return nullptr;
  }

  return new SetTiltAnglesWidget(this, vol, parent);
}

QJsonObject SetTiltAnglesTransform::serialize() const
{
  auto json = TransformNode::serialize();
  QJsonObject angles;
  for (auto it = m_tiltAngles.constBegin(); it != m_tiltAngles.constEnd();
       ++it) {
    angles[QString::number(static_cast<qulonglong>(it.key()))] = it.value();
  }
  json["angles"] = angles;

  if (!m_scanIds.isEmpty()) {
    QJsonArray scanIdsArray;
    for (int id : m_scanIds) {
      scanIdsArray.append(id);
    }
    json["scanIds"] = scanIdsArray;
  }
  return json;
}

bool SetTiltAnglesTransform::deserialize(const QJsonObject& json)
{
  if (!TransformNode::deserialize(json)) {
    return false;
  }
  QMap<size_t, double> angles;
  auto obj = json.value("angles").toObject();
  for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
    angles[static_cast<size_t>(it.key().toULongLong())] =
      it.value().toDouble();
  }
  setTiltAngles(angles);

  if (json.contains("scanIds")) {
    QVector<int> ids;
    auto arr = json.value("scanIds").toArray();
    ids.reserve(arr.size());
    for (const auto& v : arr) {
      ids.push_back(v.toInt());
    }
    setScanIds(ids);
  }
  return true;
}

QMap<QString, PortData> SetTiltAnglesTransform::transform(
  const QMap<QString, PortData>& inputs)
{
  QMap<QString, PortData> outputs;

  auto it = inputs.find("volume");
  if (it == inputs.end()) {
    return outputs;
  }

  VolumeDataPtr vol;
  try {
    vol = it.value().value<VolumeDataPtr>();
  } catch (const std::bad_any_cast&) {
    return outputs;
  }

  if (!vol || !vol->imageData()) {
    return outputs;
  }

  // Build the full tilt angle vector from the sparse map
  int numSlices = vol->dimensions()[2];
  QVector<double> angles(numSlices, 0.0);
  for (auto jt = m_tiltAngles.constBegin(); jt != m_tiltAngles.constEnd();
       ++jt) {
    if (static_cast<int>(jt.key()) < numSlices) {
      angles[static_cast<int>(jt.key())] = jt.value();
    }
  }

  // Deep-copy the vtkImageData so we don't mutate the upstream port's data.
  // Tilt angles are stored in the vtkImageData's field data, so a shallow
  // share would leak the mutation upstream.
  vtkNew<vtkImageData> outputImage;
  outputImage->DeepCopy(vol->imageData());

  auto output = std::make_shared<VolumeData>(outputImage.Get());
  output->setLabel(vol->label());
  output->setUnits(vol->units());
  output->setTiltAngles(angles);

  if (!m_scanIds.isEmpty()) {
    output->setScanIds(m_scanIds);
  }

  outputs["output"] = PortData(std::any(output), PortType::TiltSeries);
  return outputs;
}

} // namespace pipeline
} // namespace tomviz
