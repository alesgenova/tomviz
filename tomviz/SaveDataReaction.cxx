/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SaveDataReaction.h"

#include "ActiveObjects.h"
#include "EmdFormat.h"
#include "FileFormatManager.h"
#include "PythonWriter.h"

#include "pipeline/OutputPort.h"
#include "pipeline/PortData.h"
#include "pipeline/PortType.h"
#include "pipeline/data/VolumeData.h"

#include <vtkDataArray.h>
#include <vtkImageCast.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSMProxyManager.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMWriterFactory.h>
#include <vtkSmartPointer.h>
#include <vtkTrivialProducer.h>

#include <QCheckBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QVBoxLayout>

namespace tomviz {

static QStringList showArraySelectionDialog(const QStringList& arrayNames,
                                            QWidget* parent = nullptr)
{
  QDialog dialog(parent);
  dialog.setWindowTitle("Select Arrays to Save");

  auto* layout = new QVBoxLayout(&dialog);

  auto* label = new QLabel("Select which arrays to include:");
  layout->addWidget(label);

  auto* scrollArea = new QScrollArea;
  scrollArea->setWidgetResizable(true);
  auto* scrollWidget = new QWidget;
  auto* scrollLayout = new QVBoxLayout(scrollWidget);

  QList<QCheckBox*> checkboxes;
  for (const auto& name : arrayNames) {
    auto* cb = new QCheckBox(name);
    cb->setChecked(true);
    scrollLayout->addWidget(cb);
    checkboxes.append(cb);
  }
  scrollLayout->addStretch();

  scrollArea->setWidget(scrollWidget);
  layout->addWidget(scrollArea);

  auto* buttons =
    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog,
                   &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog,
                   &QDialog::reject);
  layout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) {
    return {};
  }

  QStringList selected;
  for (auto* cb : checkboxes) {
    if (cb->isChecked()) {
      selected << cb->text();
    }
  }

  return selected;
}

static QString showSingleArraySelectionDialog(const QStringList& arrayNames,
                                               const QString& activeArray,
                                               QWidget* parent = nullptr)
{
  QDialog dialog(parent);
  dialog.setWindowTitle("Select Array to Save");

  auto* layout = new QVBoxLayout(&dialog);

  auto* label = new QLabel("This format supports a single array.\n"
                           "Select which array to save:");
  layout->addWidget(label);

  auto* scrollArea = new QScrollArea;
  scrollArea->setWidgetResizable(true);
  auto* scrollWidget = new QWidget;
  auto* scrollLayout = new QVBoxLayout(scrollWidget);

  QList<QRadioButton*> radioButtons;
  for (const auto& name : arrayNames) {
    auto* rb = new QRadioButton(name);
    if (name == activeArray) {
      rb->setChecked(true);
    }
    scrollLayout->addWidget(rb);
    radioButtons.append(rb);
  }

  if (!radioButtons.isEmpty()) {
    bool anyChecked = false;
    for (auto* rb : radioButtons) {
      if (rb->isChecked()) {
        anyChecked = true;
        break;
      }
    }
    if (!anyChecked) {
      radioButtons.first()->setChecked(true);
    }
  }

  scrollLayout->addStretch();
  scrollArea->setWidget(scrollWidget);
  layout->addWidget(scrollArea);

  auto* buttons =
    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog,
                   &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog,
                   &QDialog::reject);
  layout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) {
    return {};
  }

  for (auto* rb : radioButtons) {
    if (rb->isChecked()) {
      return rb->text();
    }
  }

  return {};
}

static vtkSmartPointer<vtkImageData> filterArrays(
  vtkImageData* imageData, const QStringList& selectedArrays)
{
  vtkNew<vtkImageData> filtered;
  filtered->CopyStructure(imageData);
  filtered->GetFieldData()->ShallowCopy(imageData->GetFieldData());

  for (const auto& name : selectedArrays) {
    auto* array = imageData->GetPointData()->GetArray(name.toUtf8().data());
    if (array) {
      filtered->GetPointData()->AddArray(array);
    }
  }

  auto* origScalars = imageData->GetPointData()->GetScalars();
  if (origScalars && selectedArrays.contains(origScalars->GetName())) {
    filtered->GetPointData()->SetActiveScalars(origScalars->GetName());
  } else if (filtered->GetPointData()->GetNumberOfArrays() > 0) {
    filtered->GetPointData()->SetActiveScalars(
      filtered->GetPointData()->GetArrayName(0));
  }

  return filtered;
}

SaveDataReaction::SaveDataReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  auto& ao = ActiveObjects::instance();
  connect(&ao, &ActiveObjects::activeTipOutputPortChanged, this,
          [this](pipeline::OutputPort* port) {
            if (port) {
              connect(port, &pipeline::OutputPort::dataChanged, this,
                      &SaveDataReaction::updateEnableState,
                      Qt::UniqueConnection);
            }
            updateEnableState();
          });
  connect(&ao, &ActiveObjects::activeNodeChanged, this,
          &SaveDataReaction::updateEnableState);
  updateEnableState();
}

void SaveDataReaction::updateEnableState()
{
  auto* tipPort = ActiveObjects::instance().activeTipOutputPort();
  if (!tipPort || !tipPort->hasData()) {
    parentAction()->setEnabled(false);
    return;
  }

  auto portData = tipPort->data();
  parentAction()->setEnabled(pipeline::isVolumeType(portData.type()));
}

void SaveDataReaction::onTriggered()
{
  QStringList filters;
  filters << "EMD format (*.emd *.hdf5)"
          << "TIFF format (*.tiff)"
          << "HDF5 format (*.h5)"
          << "CSV File (*.csv)"
          << "Exodus II File (*.e *.ex2 *.ex2v2 *.exo *.exoII *.exoii *.g)"
          << "Legacy VTK Files (*.vtk)"
          << "Meta Image Files (*.mhd)"
          << "ParaView Data Files (*.pvd)"
          << "VTK ImageData Files (*.vti)"
          << "XDMF Data File (*.xmf)"
          << "JSON Image Files (*.json)";

  for (auto* writer : FileFormatManager::instance().pythonWriterFactories()) {
    filters << writer->getFileDialogFilter();
  }

  QFileDialog dialog(nullptr);
  dialog.setFileMode(QFileDialog::AnyFile);
  dialog.setNameFilters(filters);
  dialog.setObjectName("FileOpenDialog-tomviz");
  dialog.setAcceptMode(QFileDialog::AcceptSave);

  if (dialog.exec() == QDialog::Accepted) {
    QStringList filenames = dialog.selectedFiles();
    QString format = dialog.selectedNameFilter();
    QString filename = filenames[0];
    int startPos = format.indexOf("(") + 1;
    int n = format.indexOf(")") - startPos;
    QString extensionString = format.mid(startPos, n);
    QStringList extensions = extensionString.split(
      QRegularExpression(" ?\\*"), Qt::SkipEmptyParts);
    bool hasExtension = false;
    for (const QString& str : extensions) {
      if (filename.endsWith(str)) {
        hasExtension = true;
      }
    }
    if (!hasExtension) {
      filename = QString("%1%2").arg(filename, extensions[0]);
    }
    saveData(filename);
  }
}

bool SaveDataReaction::saveData(const QString& filename)
{
  auto& ao = ActiveObjects::instance();
  auto* tipPort = ao.activeTipOutputPort();
  if (!tipPort || !tipPort->hasData()) {
    qCritical("SaveDataReaction: no data to save.");
    return false;
  }

  auto portData = tipPort->data();
  if (!pipeline::isVolumeType(portData.type())) {
    qCritical("SaveDataReaction: only volume data can be saved.");
    return false;
  }

  auto volumeData = portData.value<pipeline::VolumeDataPtr>();
  if (!volumeData || !volumeData->isValid()) {
    qCritical("SaveDataReaction: invalid volume data.");
    return false;
  }

  vtkImageData* imageData = volumeData->imageData();
  vtkSmartPointer<vtkImageData> dataToSave;

  QFileInfo info(filename);
  QString ext = info.suffix().toLower();
  bool isEmd = (ext == "emd" || ext == "hdf5" || ext == "h5");

  int numArrays = imageData->GetPointData()->GetNumberOfArrays();
  if (numArrays > 1) {
    QStringList allArrays;
    for (int i = 0; i < numArrays; ++i) {
      allArrays << imageData->GetPointData()->GetArrayName(i);
    }

    if (isEmd) {
      QStringList selected = showArraySelectionDialog(allArrays);
      if (selected.isEmpty()) {
        return false;
      }

      if (selected.size() < numArrays) {
        dataToSave = filterArrays(imageData, selected);
      } else {
        dataToSave = imageData;
      }
    } else {
      QString activeName;
      auto* scalars = imageData->GetPointData()->GetScalars();
      if (scalars) {
        activeName = scalars->GetName();
      }

      QString selected =
        showSingleArraySelectionDialog(allArrays, activeName);
      if (selected.isEmpty()) {
        return false;
      }

      dataToSave = filterArrays(imageData, { selected });
    }
  } else {
    dataToSave = imageData;
  }

  if (ext == "emd" || ext == "hdf5" || ext == "h5") {
    return EmdFormat::write(filename.toStdString(), dataToSave);
  }

  auto* pyWriterFactory =
    FileFormatManager::instance().pythonWriterFactory(ext);
  if (pyWriterFactory) {
    auto writer = pyWriterFactory->createWriter();
    return writer.write(filename, dataToSave);
  }

  if (ext == "tiff" || ext == "tif") {
    auto* scalars = dataToSave->GetPointData()->GetScalars();
    if (scalars && scalars->GetDataType() == VTK_DOUBLE) {
      vtkNew<vtkImageCast> cast;
      cast->SetInputData(dataToSave);
      cast->SetOutputScalarTypeToFloat();
      cast->Update();
      dataToSave = cast->GetOutput();
    }
  }

  auto* pxm =
    vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager();

  vtkSmartPointer<vtkSMSourceProxy> producerProxy;
  producerProxy.TakeReference(vtkSMSourceProxy::SafeDownCast(
    pxm->NewProxy("sources", "TrivialProducer")));

  auto* tp =
    vtkTrivialProducer::SafeDownCast(producerProxy->GetClientSideObject());
  tp->SetOutput(dataToSave);
  producerProxy->UpdateVTKObjects();
  producerProxy->UpdatePipeline();

  auto* writerFactory =
    vtkSMProxyManager::GetProxyManager()->GetWriterFactory();
  vtkSmartPointer<vtkSMProxy> writerProxy;
  writerProxy.TakeReference(
    writerFactory->CreateWriter(filename.toUtf8().data(), producerProxy, 0));

  if (!writerProxy) {
    qCritical() << "No suitable writer found for:" << filename;
    return false;
  }

  writerProxy->UpdateVTKObjects();
  vtkSMSourceProxy::SafeDownCast(writerProxy)->UpdatePipeline();

  return true;
}

} // end of namespace tomviz
