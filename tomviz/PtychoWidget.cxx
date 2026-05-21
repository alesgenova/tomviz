/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PtychoWidget.h"
#include "ui_PtychoWidget.h"

#include "PythonUtilities.h"
#include "Utilities.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QCheckBox>
#include <QComboBox>
#include <QBrush>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QSignalBlocker>

namespace tomviz {

class PtychoWidget::Internal : public QObject
{
  Q_OBJECT

public:
  Ui::PtychoWidget ui;
  QPointer<PtychoWidget> parent;

  bool ptychoguiIsRunning = false;

  // Key is SID
  QMap<long, QStringList> versionOptions;
  // First key for these is the SID. Second key is the version.
  QMap<long, QMap<QString, double>> angleOptions;
  QMap<long, QMap<QString, QString>> allErrorLists;

  QList<long> sidList;
  QList<double> angleList;
  QStringList versionList;
  QList<bool> useList;
  QStringList errorReasonList;

  QList<long> filteredSidList;

  QMap<int, QString> tableColumns;

  Python::Module ptychoModule;

  Internal(PtychoWidget* p)
    : parent(p)
  {
    ui.setupUi(p);
    setParent(p);

    importModule();

    setupTable();
    setupConnections();
  }

  void setupConnections()
  {
    connect(ui.startPtychoGUI, &QPushButton::clicked, this,
            &Internal::startPtychoGUI);

    connect(ui.ptychoDirectory, &QLineEdit::editingFinished,
            this, &Internal::ptychoDirEdited);
    connect(ui.selectPtychoDirectory, &QPushButton::clicked, this,
            &Internal::selectPtychoDirectory);

    connect(ui.loadFromCSVFile, &QLineEdit::editingFinished,
            this, &Internal::setUseAndVersionsFromCSV);
    connect(ui.selectLoadFromCSVFile, &QPushButton::clicked, this,
            &Internal::selectLoadFromCSV);

    connect(ui.filterSIDsString, &QLineEdit::editingFinished,
            this, &Internal::updateFilteredSidList);
    connect(ui.loadSidsFromTxt, &QPushButton::clicked, this,
            &Internal::onLoadSidsFromTxtClicked);

    connect(ui.selectOutputInfoFile, &QPushButton::clicked, this,
            &Internal::selectOutputInfoFile);
  }

  void setupTable()
  {
    auto* table = ui.table;
    auto& columns = tableColumns;

    columns.clear();
    columns[0] = "SID";
    columns[1] = "Angle";
    columns[2] = "Version";
    columns[3] = "Use";
    columns[4] = "Error Reason";

    table->setColumnCount(columns.size());
    for (int i = 0; i < columns.size(); ++i) {
      auto* header = new QTableWidgetItem(columns[i]);
      table->setHorizontalHeaderItem(i, header);
    }

    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);

    table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table, &QWidget::customContextMenuRequested, this,
            &Internal::showTableContextMenu);
  }

  void showTableContextMenu(const QPoint& pos)
  {
    auto* table = ui.table;
    auto selectedRows = table->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
      return;
    }

    QSet<QString> commonVersions;
    bool first = true;
    for (auto& index : selectedRows) {
      int row = index.row();
      auto sid = filteredSidList[row];
      QSet<QString> versions(versionOptions[sid].begin(),
                             versionOptions[sid].end());
      if (first) {
        commonVersions = versions;
        first = false;
      } else {
        commonVersions &= versions;
      }
    }

    QMenu menu(table);
    auto* setVersionAction = menu.addAction("Set Version...");
    if (commonVersions.isEmpty()) {
      setVersionAction->setEnabled(false);
      setVersionAction->setText("Set Version... (no shared versions)");
    }
    auto* chosen = menu.exec(table->viewport()->mapToGlobal(pos));
    if (chosen != setVersionAction) {
      return;
    }

    QStringList sortedVersions = commonVersions.values();
    sortedVersions.sort();

    int defaultIndex = 0;
    QSet<QString> currentVersions;
    for (auto& index : selectedRows) {
      int row = index.row();
      auto sid = filteredSidList[row];
      auto idx = sidList.indexOf(sid);
      currentVersions.insert(versionList[idx]);
    }
    if (currentVersions.size() == 1) {
      int idx = sortedVersions.indexOf(*currentVersions.begin());
      if (idx >= 0) {
        defaultIndex = idx;
      }
    }

    bool ok = false;
    auto version = QInputDialog::getItem(
      parent, "Set Version", "Version:", sortedVersions, defaultIndex,
      false, &ok);
    if (!ok) {
      return;
    }

    for (auto& index : selectedRows) {
      int row = index.row();
      auto sid = filteredSidList[row];
      auto idx = sidList.indexOf(sid);
      versionList[idx] = version;
    }

    onSelectedVersionsChanged();
    updateTable();
  }

  void importModule()
  {
    Python python;

    if (ptychoModule.isValid()) {
      return;
    }

    ptychoModule = python.import("tomviz.ptycho");
    if (!ptychoModule.isValid()) {
      qCritical() << "Failed to import \"tomviz.ptycho\" module";
    }
  }

  QList<long> selectedSids()
  {
    // Only include the filtered ones
    QList<long> ret;
    for (auto& sid : filteredSidList) {
      auto idx = sidList.indexOf(sid);
      auto use = useList[idx];
      if (use) {
        ret.append(sid);
      }
    }

    return ret;
  }

  QStringList selectedVersions()
  {
    QStringList versions;
    for (auto sid : selectedSids()) {
      auto idx = sidList.indexOf(sid);
      versions.append(versionList[idx]);
    }
    return versions;
  }

  QList<double> selectedAngles()
  {
    QList<double> angles;
    for (auto sid : selectedSids()) {
      auto idx = sidList.indexOf(sid);
      angles.append(angleList[idx]);
    }
    return angles;
  }

  QList<long> invalidSidsSelected()
  {
    QList<long> invalid;
    for (auto sid : selectedSids()) {
      auto idx = sidList.indexOf(sid);
      if (!errorReasonList[idx].isEmpty()) {
        invalid.append(sid);
      }
    }
    return invalid;
  }

  bool validate(QString& reason)
  {
    // Validate settings
    if (ptychoDirectory().isEmpty() || !QDir(ptychoDirectory()).exists()) {
      reason = "Ptycho directory does not exist: " + ptychoDirectory();
      return false;
    }

    if (sidList.isEmpty()) {
      reason = "No SIDs found in ptycho directory: " + ptychoDirectory();
      return false;
    }

    auto invalid = invalidSidsSelected();
    if (!invalid.isEmpty()) {
      QString title = "Invalid SID and version combinations selected";
      QString text = "Invalid SIDs were selected. ";
      text += "Do you wish to automatically deselect them and continue?";
      if (QMessageBox::question(parent, title, text) == QMessageBox::No) {
        reason = "Invalid SIDs were selected";
        return false;
      }

      for (auto sid : invalid) {
        auto idx = sidList.indexOf(sid);
        useList[idx] = false;
        updateTable();
      }
    }

    return true;
  }

  void updateTable()
  {
    auto* table = ui.table;

    int scrollbarPosition = 0;
    auto scrollbar = table->verticalScrollBar();
    if (scrollbar) {
      scrollbarPosition = scrollbar->value();
    }

    table->clearContents();

    table->setRowCount(filteredSidList.size());
    for (int i = 0; i < filteredSidList.size(); ++i) {
      auto sid = filteredSidList[i];
      bool invalid = false;
      for (auto j : tableColumns.keys()) {
        auto column = tableColumns[j];
        auto value = tableValue(sid, column);
        if (column == "Version") {
          auto* cb = createVersionComboBox(sid, value);
          table->setCellWidget(i, j, cb);
          continue;
        } else if (column == "Use") {
          auto* w = createUseCheckBox(sid, value);
          table->setCellWidget(i, j, w);
          continue;
        } else if (column == "Error Reason") {
          invalid = !value.isEmpty();
        }

        auto* item = new QTableWidgetItem(value);
        item->setTextAlignment(Qt::AlignCenter);
        table->setItem(i, j, item);
      }

      if (invalid) {
        // Make every item have a red background
        for (int j = 0; j < tableColumns.size(); ++j) {
          auto* item = table->item(i, j);
          if (item) {
            item->setBackground(QBrush(Qt::red));
          } else {
            auto* cw = table->cellWidget(i, j);
            if (cw) {
              cw->setStyleSheet("background-color: red");
            }
          }
        }
      }
    }

    if (scrollbar) {
      scrollbar->setValue(scrollbarPosition);
    }
  }

  QWidget* createVersionComboBox(long sid, QString value)
  {
    if (versionOptions[sid].size() < 2) {
      // If there aren't any options, the item will just be a label
      QString text = "None";
      if (versionOptions[sid].size() == 1) {
        text = versionOptions[sid][0];
      }
      return createTableWidget(new QLabel(text, parent));
    }

    auto cb = new QComboBox(parent);
    for (auto& option: versionOptions[sid]) {
      cb->addItem(option);
    }
    cb->setCurrentText(value);

    connect(cb, &QComboBox::currentIndexChanged, this, [this, sid, cb]() {
      auto idx = sidList.indexOf(sid);
      versionList[idx] = cb->currentText();
      onSelectedVersionsChanged();
      // Update the table, because the angle and error reason likely changed
      updateTable();
    });

    return createTableWidget(cb);
  }

  QWidget* createUseCheckBox(long sid, QString value)
  {
    auto cb = new QCheckBox(parent);
    cb->setChecked(value == "x" || value == "1");
    connect(cb, &QCheckBox::toggled, this, [this, sid](bool b) {
      auto idx = sidList.indexOf(sid);
      useList[idx] = b;
    });

    return createTableWidget(cb);
  }

  QWidget* createTableWidget(QWidget* w)
  {
    // This is required to center the widget
    auto* tw = new QWidget(ui.table);
    auto* layout = new QHBoxLayout(tw);
    layout->addWidget(w);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    return tw;
  }

  QString tableValue(long sid, QString column)
  {
    auto idx = sidList.indexOf(sid);
    if (column == "SID") {
      return QString::number(sidList[idx]);
    } else if (column == "Version") {
      return versionList[idx];
    } else if (column == "Angle") {
      return QString::number(angleList[idx]);
    } else if (column == "Use") {
      return useList[idx] ? "x" : "";
    } else if (column == "Error Reason") {
      return errorReasonList[idx];
    }

    qCritical() << "Unknown table column: " << column;
    return "";
  }

  QString defaultOutputInfoFile() { return ""; }

  void readSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("ptycho");
    settings->beginGroup("process");

    setPtychoGUICommand(
      settings->value("ptychoGUICommand", "run-ptycho").toString());

    auto savedPtychoDir = settings->value("ptychoDirectory", "").toString();
    if (!savedPtychoDir.isEmpty() && !QDir(savedPtychoDir).exists()) {
      savedPtychoDir = "";
    }
    setPtychoDirectory(savedPtychoDir);
    setCsvFile(settings->value("loadFromCSVFile", "").toString());
    setFilterSIDsString(settings->value("filterSIDsString", "").toString());

    setOutputInfoFile(
      settings->value("outputInfoFile", defaultOutputInfoFile()).toString());
    setRotateDatasets(
      settings->value("rotateDatasets", true).toBool());

    QVariantList sidListV = settings->value("sidListV").toList();
    QVariantList versionListV = settings->value("versionListV").toList();
    QVariantList useListV = settings->value("useListV").toList();

    QList<long> savedSidList;
    for (const auto& var : sidListV) {
      savedSidList.append(var.value<long>());
    }

    QStringList savedVersionList;
    for (const auto& var : versionListV) {
      savedVersionList.append(var.value<QString>());
    }

    QList<bool> savedUseList;
    for (const auto& var : useListV) {
      savedUseList.append(var.value<bool>());
    }

    settings->endGroup();
    settings->endGroup();

    if (!ptychoDirectory().isEmpty()) {
      // Trigger a load
      loadPtychoDir();

      if (!csvFile().isEmpty()) {
        // Trigger applying the CSV file
        setUseAndVersionsFromCSV();
      }

      if (!filterSIDsString().isEmpty()) {
        // Trigger an update via the filters
        updateFilteredSidList();
      }

      if (savedSidList == sidList) {
        // If the saved SID list matches, we can also load the settings
        // for "use" and "version"
        versionList = savedVersionList;
        useList = savedUseList;
        onSelectedVersionsChanged();
        updateTable();
      }
    }
  }

  void writeSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("ptycho");
    settings->beginGroup("process");

    // Save general settings
    settings->setValue("ptychoGUICommand", ptychoGUICommand());
    settings->setValue("ptychoDirectory", ptychoDirectory());
    settings->setValue("loadFromCSVFile", csvFile());

    settings->setValue("filterSIDsString", filterSIDsString());

    settings->setValue("outputInfoFile", outputInfoFile());
    settings->setValue("rotateDatasets", rotateDatasets());

    // Save out our lists
    QVariantList sidListV;
    for (auto v: sidList) {
      sidListV.append(QVariant::fromValue(v));
    }

    QVariantList versionListV;
    for (auto v: versionList) {
      versionListV.append(QVariant::fromValue(v));
    }

    QVariantList useListV;
    for (auto b: useList) {
      useListV.append(QVariant::fromValue(b));
    }

    settings->setValue("sidListV", sidListV);
    settings->setValue("versionListV", versionListV);
    settings->setValue("useListV", useListV);

    settings->endGroup();
    settings->endGroup();
  }

  void startPtychoGUI()
  {
    if (ptychoguiIsRunning) {
      // It's already running. Just return.
      return;
    }

    QString program = ptychoGUICommand();
    QStringList args;

    auto* process = new QProcess(this);

    auto processEnv = QProcessEnvironment::systemEnvironment();

    // Remove variables related to python environment
    processEnv.remove("PYTHONHOME");
    processEnv.remove("PYTHONPATH");

    process->setProcessEnvironment(processEnv);

    // Forward stdout/stderr to this process
    process->setProcessChannelMode(QProcess::ForwardedChannels);

    process->start(program, args);

    ptychoguiIsRunning = true;

    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this]() {
      ptychoguiIsRunning = false;
      loadPtychoDir();
    });

    connect(
      process, &QProcess::errorOccurred, this,
      [this, process](QProcess::ProcessError err) {
        ptychoguiIsRunning = false;

        QString title;
        QString msg;

        if (err == QProcess::FailedToStart) {
          title = "Ptycho GUI failed to start";
          msg = QString("The program \"%1\" failed to start.\n\n")
                  .arg(process->program());
        } else {
          QString output = process->readAllStandardOutput();
          QString error = process->readAllStandardError();
          title = "Ptycho GUI exited with an error";
          msg =
            QString("stdout: \"%1\"\n\nstderr: \"%2\"").arg(output).arg(error);
        }
        QMessageBox::critical(parent.data(), title, msg);
      });
  }

  void selectPtychoDirectory()
  {
    QString caption = "Select Ptycho GUI Directory";
    auto file =
      QFileDialog::getExistingDirectory(parent.data(), caption,
                                        ptychoDirectory());
    if (file.isEmpty()) {
      return;
    }

    // If "recon_result" exists underneath the selected directory,
    // it means the parent directory was selected.
    // We should automatically select the child one.
    auto possibleChildPath = QDir(file).filePath("recon_result");
    if (QFile::exists(possibleChildPath)) {
      file = possibleChildPath;
    }

    setPtychoDirectory(file);
    ptychoDirEdited();
  }

  void ptychoDirEdited()
  {
    auto dir = ptychoDirectory();
    if (!dir.isEmpty() && !QDir(dir).exists()) {
      QMessageBox::critical(parent.data(), "Directory Not Found",
                            "Ptycho directory does not exist: " + dir);
      setPtychoDirectory("");
      setCsvFile("");
      setFilterSIDsString("");
      return;
    }

    // Whenever this is called, make sure we clear the CSV file and SID filters
    setCsvFile("");
    setFilterSIDsString("");

    loadPtychoDir();
  }

  void loadPtychoDir()
  {
    clearTable();

    Python python;

    auto func = ptychoModule.findFunction("gather_ptycho_info");
    if (!func.isValid()) {
      QString msg = "Failed to import \"tomviz.ptycho.gather_ptycho_info\"";
      qCritical() << msg;
      return;
    }

    Python::Dict kwargs;
    kwargs.set("ptycho_dir", ptychoDirectory());
    auto result = func.call(kwargs);

    if (!result.isValid() || !result.isDict()) {
      QString msg = "Error calling \"tomviz.ptycho.gather_ptycho_info\"";
      qCritical() << msg;
      return;
    }

    auto resultDict = result.toDict();

    auto sidListV = resultDict["sid_list"].toVariant().toList();
    auto versionDictV = resultDict["version_list"].toVariant().toList();
    auto angleDictV = resultDict["angle_list"].toVariant().toList();
    auto errorDictV = resultDict["error_list"].toVariant().toList();

    sidList.clear();
    versionOptions.clear();
    angleOptions.clear();
    allErrorLists.clear();
    for (size_t i = 0; i < sidListV.size(); ++i) {
      auto sid = sidListV[i].toLong();
      sidList.append(sid);

      auto versionOptionsV = versionDictV[i].toList();
      auto theseAnglesV = angleDictV[i].toList();
      auto theseErrorsV = errorDictV[i].toList();

      QStringList versions;
      QMap<QString, double> angles;
      QMap<QString, QString> errors;
      for (size_t j = 0; j < versionOptionsV.size(); ++j) {
        auto version = QString::fromStdString(versionOptionsV[j].toString());
        versions.append(version);
        angles[version] = theseAnglesV[j].toDouble();
        errors[version] = QString::fromStdString(theseErrorsV[j].toString());
      }
      versionOptions[sid] = versions;
      angleOptions[sid] = angles;
      allErrorLists[sid] = errors;
    }

    resetSelectedVersionsAndUseList();
    updateFilteredSidList();
  }

  void resetSelectedVersionsAndUseList()
  {
    versionList.clear();
    useList.clear();

    for (auto sid: sidList) {
      bool set = false;
      for (auto& version: versionOptions[sid]) {
        if (allErrorLists[sid][version].isEmpty()) {
          // This one is valid.
          versionList.append(version);
          useList.append(true);
          set = true;
          break;
        }
      }
      if (!set) {
        // Do the first one and don't set it to be used.
        versionList.append(versionOptions[sid][0]);
        useList.append(false);
      }
    }

    onSelectedVersionsChanged();
  }

  void onSelectedVersionsChanged()
  {
    angleList.clear();
    errorReasonList.clear();

    for (int i = 0; i < sidList.size(); ++i) {
      auto sid = sidList[i];
      auto version = versionList[i];
      angleList.append(angleOptions[sid][version]);
      errorReasonList.append(allErrorLists[sid][version]);
    }
  }

  void updateFilteredSidList()
  {
    auto filterString = filterSIDsString();

    Python python;

    auto func = ptychoModule.findFunction("filter_sid_list");
    if (!func.isValid()) {
      qCritical() << "Failed to find function \"filter_sid_list\"";
      return;
    }

    Python::Dict kwargs;
    kwargs.set("sid_list", sidList);
    kwargs.set("filter_string", filterString);
    auto result = func.call(kwargs);
    if (!result.isValid() || !result.isList()) {
      qCritical() << "Failed to call function \"filter_sid_list\"";
      return;
    }

    filteredSidList.clear();
    auto resultList = result.toList();
    for (int i = 0; i < resultList.length(); ++i) {
      filteredSidList.append(resultList[i].toLong());
    }

    updateTable();
  }

  void onLoadSidsFromTxtClicked()
  {
    QString caption = "Select txt file";
    QString filter = "*.txt";
    auto startPath = ptychoDirectory();
    auto filePath =
      QFileDialog::getOpenFileName(parent.data(), caption, startPath, filter);

    if (filePath.isEmpty()) {
      return;
    }

    QFile file(filePath);
    if (!file.exists()) {
      qCritical() << QString("Txt file does not exist: %1").arg(filePath);
      return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
      qCritical()
        << QString("Failed to open file \"%1\" with error: ").arg(filePath)
        << file.errorString();
      return;
    }

    QTextStream reader(&file);

    // Now load the SIDs
    QStringList sids;
    while (!reader.atEnd()) {
      auto line = reader.readLine().trimmed();
      if (line.isEmpty() || line.startsWith('#')) {
        // Skip over it
        continue;
      }

      sids.append(line.split(' ')[0]);
    }

    ui.filterSIDsString->setText(sids.join(", "));

    updateFilteredSidList();
  }

  void selectLoadFromCSV()
  {
    QString caption = "Select CSV file to load Use and Version settings";
    auto startPath = !csvFile().isEmpty() ? csvFile() : ptychoDirectory();
    auto file =
      QFileDialog::getOpenFileName(parent.data(), caption, startPath);
    if (file.isEmpty()) {
      return;
    }
    ui.loadFromCSVFile->setText(file);

    setUseAndVersionsFromCSV();
  }

  void setUseAndVersionsFromCSV()
  {
    Python python;

    auto func = ptychoModule.findFunction("get_use_and_versions_from_csv");
    if (!func.isValid()) {
      qCritical() << "Failed to find function \"get_use_and_versions_from_csv\"";
      return;
    }

    Python::Dict kwargs;
    kwargs.set("csv_path", csvFile());
    auto result = func.call(kwargs);
    if (!result.isValid() || !result.isDict()) {
      qCritical() << "Failed to call function \"get_use_and_versions_from_csv\"";
      return;
    }

    auto resultDict = result.toDict();

    QList<long> sids;
    QList<bool> use;
    QStringList versions;

    auto sidsPy = resultDict["sids"].toList();
    auto usePy = resultDict["use"].toList();
    auto versionsPy = resultDict["versions"].toList();

    for (auto i = 0; i < sidsPy.length(); ++i) {
      sids.append(sidsPy[i].toLong());
      if (i < usePy.length()) {
        use.append(usePy[i].toBool());
      }
      if (i < versionsPy.length()) {
        versions.append(versionsPy[i].toString());
      }
    }

    if (sids.size() == 0) {
      qCritical() << "No SIDs found in CSV file. Aborting";
      return;
    }

    if (use.size() != 0) {
      // Set the "Use" for every current one to "false";
      for (int i = 0; i < useList.size(); ++i) {
        useList[i] = false;
      }
    }

    for (int i = 0; i < sids.size(); ++i) {
      auto sid = sids[i];
      auto idx = sidList.indexOf(sid);
      if (i < use.size()) {
        useList[idx] = use[i];
      }

      if (i < versions.size()) {
        // Verify it is a valid version
        auto newVersion = versions[i];
        if (!versionOptions[sid].contains(newVersion)) {
          qCritical() << "SID \"" << sid << "\" from CSV file "
                      << "indicated a version of " << newVersion << ", "
                      << "but that did not match the available versions "
                      << "found within the ptycho directory for that SID. "
                      << "Skipping...";
        } else {
          versionList[idx] = newVersion;
        }
      }
    }

    updateTable();
  }

  void selectOutputInfoFile()
  {
    QString caption = "Select output info file";
    auto startPath = outputInfoFile();
    if (startPath.isEmpty()) {
      startPath = ptychoDirectory();
    }
    auto file = QFileDialog::getSaveFileName(
      parent.data(), caption, startPath, "Text files (*.txt)");
    if (file.isEmpty()) {
      return;
    }

    setOutputInfoFile(file);
  }

  void clearTable()
  {
    versionOptions.clear();
    angleOptions.clear();
    allErrorLists.clear();

    sidList.clear();
    angleList.clear();
    versionList.clear();
    useList.clear();
    errorReasonList.clear();

    filteredSidList.clear();
  }

  QString ptychoGUICommand() const { return ui.ptychoGUICommand->text(); }

  void setPtychoGUICommand(QString s) { ui.ptychoGUICommand->setText(s); }

  QString ptychoDirectory() const { return ui.ptychoDirectory->text(); }

  void setPtychoDirectory(QString s) { ui.ptychoDirectory->setText(s); }

  QString csvFile() const { return ui.loadFromCSVFile->text(); }

  void setCsvFile(QString s) { ui.loadFromCSVFile->setText(s); }

  QString filterSIDsString() const { return ui.filterSIDsString->text().trimmed(); }

  void setFilterSIDsString(QString s) const { ui.filterSIDsString->setText(s); }

  QString outputInfoFile() const { return ui.outputInfoFile->text(); }

  void setOutputInfoFile(QString s) { ui.outputInfoFile->setText(s); }

  bool rotateDatasets() const { return ui.rotateDatasets->isChecked(); }

  void setRotateDatasets(bool b) { ui.rotateDatasets->setChecked(b); }
};

PtychoWidget::PtychoWidget(
  const QMap<QString, pipeline::PortData>& /*inputs*/, QWidget* p)
  : pipeline::CustomPythonNodeWidget(p), m_internal(new Internal(this))
{
}

PtychoWidget::~PtychoWidget() = default;

void PtychoWidget::getValues(QMap<QString, QVariant>& map)
{
  auto sids = m_internal->selectedSids();
  auto versions = m_internal->selectedVersions();
  auto angles = m_internal->selectedAngles();

  QJsonArray sidArray, versionArray, angleArray;
  for (auto sid : sids) {
    sidArray.append(static_cast<qint64>(sid));
  }
  for (const auto& v : versions) {
    versionArray.append(v);
  }
  for (auto angle : angles) {
    angleArray.append(angle);
  }

  map.insert("ptycho_dir", m_internal->ptychoDirectory());
  map.insert("output_info_file", m_internal->outputInfoFile());
  map.insert("rotate_datasets", m_internal->rotateDatasets());
  map.insert("sid_list", QString::fromUtf8(
    QJsonDocument(sidArray).toJson(QJsonDocument::Compact)));
  map.insert("version_list", QString::fromUtf8(
    QJsonDocument(versionArray).toJson(QJsonDocument::Compact)));
  map.insert("angle_list", QString::fromUtf8(
    QJsonDocument(angleArray).toJson(QJsonDocument::Compact)));

  QJsonObject uiState;
  uiState["filter_sids_string"] = m_internal->filterSIDsString();
  uiState["csv_file"] = m_internal->csvFile();

  QJsonArray fullSidList, fullVersionList, fullUseList;
  for (auto sid : m_internal->sidList) {
    fullSidList.append(static_cast<qint64>(sid));
  }
  for (const auto& v : m_internal->versionList) {
    fullVersionList.append(v);
  }
  for (auto u : m_internal->useList) {
    fullUseList.append(u);
  }
  uiState["full_sid_list"] = fullSidList;
  uiState["full_version_list"] = fullVersionList;
  uiState["full_use_list"] = fullUseList;

  map.insert("ui_state", QString::fromUtf8(
    QJsonDocument(uiState).toJson(QJsonDocument::Compact)));
}

void PtychoWidget::setValues(const QMap<QString, QVariant>& map)
{
  auto dir = map.value("ptycho_dir").toString();
  if (dir.isEmpty()) {
    m_internal->readSettings();
    return;
  }

  {
    QSignalBlocker b1(m_internal->ui.ptychoDirectory);
    m_internal->setPtychoDirectory(dir);
    m_internal->setOutputInfoFile(
      map.value("output_info_file").toString());
    m_internal->setRotateDatasets(
      map.value("rotate_datasets", true).toBool());
  }

  m_internal->loadPtychoDir();

  auto uiStateJson = map.value("ui_state").toString();
  if (!uiStateJson.isEmpty()) {
    auto uiState = QJsonDocument::fromJson(uiStateJson.toUtf8()).object();
    m_internal->setCsvFile(uiState.value("csv_file").toString());
    m_internal->setFilterSIDsString(
      uiState.value("filter_sids_string").toString());

    auto fullSidArr = uiState.value("full_sid_list").toArray();
    auto fullVersionArr = uiState.value("full_version_list").toArray();
    auto fullUseArr = uiState.value("full_use_list").toArray();

    QList<long> savedSidList;
    for (const auto& v : fullSidArr) {
      savedSidList.append(static_cast<long>(v.toInteger()));
    }
    QStringList savedVersionList;
    for (const auto& v : fullVersionArr) {
      savedVersionList.append(v.toString());
    }
    QList<bool> savedUseList;
    for (const auto& v : fullUseArr) {
      savedUseList.append(v.toBool());
    }

    if (savedSidList == m_internal->sidList) {
      m_internal->versionList = savedVersionList;
      m_internal->useList = savedUseList;
      m_internal->onSelectedVersionsChanged();
    } else if (!m_internal->csvFile().isEmpty()) {
      m_internal->setUseAndVersionsFromCSV();
    }

    m_internal->updateFilteredSidList();
  } else {
    // No ui_state -- use sid_list/version_list to restore selections
    auto sidJson = map.value("sid_list", "[]").toString();
    auto versionJson = map.value("version_list", "[]").toString();
    auto sidArr = QJsonDocument::fromJson(sidJson.toUtf8()).array();
    auto verArr = QJsonDocument::fromJson(versionJson.toUtf8()).array();

    QSet<long> selectedSids;
    QMap<long, QString> selectedVersions;
    for (int i = 0; i < sidArr.size(); ++i) {
      long sid = static_cast<long>(sidArr[i].toInteger());
      selectedSids.insert(sid);
      if (i < verArr.size()) {
        selectedVersions[sid] = verArr[i].toString();
      }
    }

    for (int i = 0; i < m_internal->sidList.size(); ++i) {
      auto sid = m_internal->sidList[i];
      m_internal->useList[i] = selectedSids.contains(sid);
      if (selectedVersions.contains(sid)) {
        m_internal->versionList[i] = selectedVersions[sid];
      }
    }
    m_internal->onSelectedVersionsChanged();
    m_internal->updateFilteredSidList();
  }
}

void PtychoWidget::writeSettings()
{
  m_internal->writeSettings();
}

} // namespace tomviz

#include "PtychoWidget.moc"
