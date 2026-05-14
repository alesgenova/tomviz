/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PyXRFDialog.h"
#include "ui_PyXRFDialog.h"

#include "PythonUtilities.h"
#include "Utilities.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QCheckBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>

namespace {

bool executableExists(const QString& command)
{
  if (command.isEmpty()) {
    return false;
  }
  if (command.contains('/') || command.contains(QDir::separator())) {
    QFileInfo info(command);
    return info.isFile() && info.isExecutable();
  }
  return !QStandardPaths::findExecutable(command).isEmpty();
}

QString findPyxrfUtilsCommand(const QString& savedCommand)
{
  if (executableExists(savedCommand)) {
    return savedCommand;
  }

  const QStringList candidates = { "run-pyxrf-utils", "pyxrf-utils" };
  for (const auto& candidate : candidates) {
    if (executableExists(candidate)) {
      return candidate;
    }
  }

  const QString absoluteFallback =
    "/nsls2/data2/hxn/legacy/Hiran/tomviz/conda_envs/"
    "tomviz-latest-wip/bin/run-pyxrf-utils";
  if (executableExists(absoluteFallback)) {
    return absoluteFallback;
  }

  return "";
}

} // anonymous namespace

namespace tomviz {

class PyXRFDialog::Internal : public QObject
{
public:
  Ui::PyXRFDialog ui;
  QPointer<PyXRFDialog> parent;

  bool pyxrfIsRunning = false;

  struct ScanEntry
  {
    int scanId;
    double theta;
    QString status;
    bool use;
  };
  QList<ScanEntry> scanEntries;

  Python::Module pyxrfModule;

  Internal(PyXRFDialog* p) : parent(p)
  {
    ui.setupUi(p);
    setParent(p);

    setupTableColumns();
    setupConnections();
  }

  void setupConnections()
  {
    connect(ui.selectWorkingDirectory, &QPushButton::clicked, this,
            &Internal::selectWorkingDirectory);
    connect(ui.selectParametersFile, &QPushButton::clicked, this,
            &Internal::selectParametersFile);
    connect(ui.selectCsvOutput, &QPushButton::clicked, this,
            &Internal::selectCsvOutput);

    connect(ui.downloadData, &QPushButton::clicked, this,
            &Internal::onDownloadData);

    connect(ui.loadSidsFromTxtOrCSV, &QPushButton::clicked, this,
            &Internal::onLoadSidsFromTxt);
    connect(ui.applyFilter, &QPushButton::clicked, this,
            &Internal::applyFilter);

    connect(ui.startPyXRFGUI, &QPushButton::clicked, this,
            &Internal::startPyXRFGUI);

    connect(ui.buttonBox, &QDialogButtonBox::accepted, this,
            &Internal::accepted);
    connect(ui.buttonBox, &QDialogButtonBox::helpRequested, this,
            []() {
              openHelpUrl(
                "https://tomviz.readthedocs.io/en/latest/workflows_pyxrf.html");
            });

    connect(ui.workingDirectory, &QLineEdit::editingFinished, this,
            &Internal::onDirectoryOrRangeChanged);
    connect(ui.scanRange, &QLineEdit::editingFinished, this,
            &Internal::onDirectoryOrRangeChanged);
  }

  void setupTableColumns()
  {
    auto* table = ui.scanTable;
    table->setColumnCount(4);
    table->setHorizontalHeaderItem(0, new QTableWidgetItem("Scan ID"));
    table->setHorizontalHeaderItem(1, new QTableWidgetItem("Theta"));
    table->setHorizontalHeaderItem(2, new QTableWidgetItem("Status"));
    table->setHorizontalHeaderItem(3, new QTableWidgetItem("Use"));
  }

  void setupComboBoxes()
  {
    ui.icName->clear();
    auto names = icNames();
    if (names.isEmpty()) {
      names.append("sclr1_ch4");
    }
    ui.icName->addItems(names);
    int idx = ui.icName->findText("sclr1_ch4");
    if (idx >= 0) {
      ui.icName->setCurrentIndex(idx);
    }
  }

  void importModule()
  {
    Python python;
    if (pyxrfModule.isValid()) {
      return;
    }
    pyxrfModule = python.import("tomviz.pyxrf");
    if (!pyxrfModule.isValid()) {
      qCritical() << "Failed to import \"tomviz.pyxrf\" module";
    }
  }

  // --- Accessors ---

  QString command() const { return ui.command->text(); }
  void setCommand(const QString& s) { ui.command->setText(s); }

  QString workingDirectory() const { return ui.workingDirectory->text(); }
  void setWorkingDirectory(const QString& s)
  {
    ui.workingDirectory->setText(s);
  }

  QString scanRange() const { return ui.scanRange->text().trimmed(); }
  void setScanRange(const QString& s) { ui.scanRange->setText(s); }

  bool redownloadSuccessful() const { return ui.redownloadSuccessful->isChecked(); }
  void setRedownloadSuccessful(bool b) { ui.redownloadSuccessful->setChecked(b); }

  QString parametersFile() const { return ui.parametersFile->text(); }
  void setParametersFile(const QString& s) { ui.parametersFile->setText(s); }

  QString icName() const { return ui.icName->currentText(); }
  void setIcName(const QString& s) { ui.icName->setCurrentText(s); }

  bool skipProcessed() const { return ui.skipProcessed->isChecked(); }
  void setSkipProcessed(bool b) { ui.skipProcessed->setChecked(b); }

  bool rotateDatasets() const { return ui.rotateDatasets->isChecked(); }
  void setRotateDatasets(bool b) { ui.rotateDatasets->setChecked(b); }

  QString csvOutput() const { return ui.csvOutput->text().trimmed(); }
  void setCsvOutput(const QString& s) { ui.csvOutput->setText(s); }

  QString pyxrfGUICommand() const { return ui.pyxrfGUICommand->text(); }
  void setPyxrfGUICommand(const QString& s)
  {
    ui.pyxrfGUICommand->setText(s);
  }

  QString skipScanIds() const
  {
    QJsonArray arr;
    for (const auto& entry : scanEntries) {
      if (!entry.use) {
        arr.append(entry.scanId);
      }
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
  }

  // --- Helpers ---

  QString defaultWorkingDirectory() const
  {
    return QDir::home().filePath("data");
  }

  // --- UI Slots ---

  void selectWorkingDirectory()
  {
    auto dir = QFileDialog::getExistingDirectory(
      parent.data(), "Select data directory", workingDirectory());
    if (!dir.isEmpty()) {
      setWorkingDirectory(dir);
      onDirectoryOrRangeChanged();
    }
  }

  void onDirectoryOrRangeChanged()
  {
    populateScanTable();
    setupComboBoxes();
    autoApplyFilter();
  }

  void selectParametersFile()
  {
    auto startPath =
      parametersFile().isEmpty() ? workingDirectory() : parametersFile();
    auto file = QFileDialog::getOpenFileName(
      parent.data(), "Select parameters file", startPath, "*.json");
    if (!file.isEmpty()) {
      setParametersFile(file);
    }
  }

  void selectCsvOutput()
  {
    auto startPath =
      csvOutput().isEmpty() ? workingDirectory() : csvOutput();
    auto file = QFileDialog::getSaveFileName(
      parent.data(), "Select output CSV file", startPath,
      "CSV Files (*.csv)");
    if (!file.isEmpty()) {
      setCsvOutput(file);
    }
  }

  // --- Download & Table ---

  void onDownloadData()
  {
    auto range = scanRange();
    if (range.isEmpty()) {
      QMessageBox::warning(parent.data(), "Missing Scan Range",
                           "Please enter a scan range before downloading.");
      return;
    }

    auto cmd = command();
    if (!executableExists(cmd)) {
      QMessageBox::critical(
        parent.data(), "Command Not Found",
        QString("The pyxrf-utils executable \"%1\" was not found.")
          .arg(cmd.isEmpty() ? QString("(empty)") : cmd));
      return;
    }

    QStringList args = { "make-hdf5", workingDirectory(), "--range", range };
    if (redownloadSuccessful()) {
      args.append("--force");
    }

    auto* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    ui.downloadData->setEnabled(false);
    ui.downloadData->setText("Downloading...");

    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process](int exitCode, QProcess::ExitStatus) {
              ui.downloadData->setEnabled(true);
              ui.downloadData->setText("Download Data");
              process->deleteLater();
              if (exitCode == 0) {
                populateScanTable();
                setupComboBoxes();
              } else {
                QMessageBox::warning(parent.data(), "Download Failed",
                                     "pyxrf-utils make-hdf5 failed. "
                                     "Check the terminal output for details.");
              }
            });

    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError) {
              ui.downloadData->setEnabled(true);
              ui.downloadData->setText("Download Data");
              QMessageBox::critical(
                parent.data(), "Download Failed",
                QString("Failed to start \"%1\"").arg(process->program()));
              process->deleteLater();
            });

    process->start(cmd, args);
  }

  void populateScanTable()
  {
    scanEntries.clear();

    auto wd = workingDirectory();
    if (wd.isEmpty() || !QDir(wd).exists()) {
      rebuildTableUI();
      return;
    }

    importModule();

    Python python;
    auto func = pyxrfModule.findFunction("read_scan_metadata");
    if (!func.isValid()) {
      qCritical() << "Failed to find tomviz.pyxrf.read_scan_metadata";
      rebuildTableUI();
      return;
    }

    Python::Dict kwargs;
    kwargs.set("working_directory", wd);
    kwargs.set("scan_range", scanRange());
    auto res = func.call(kwargs);
    if (!res.isValid() || !res.isList()) {
      rebuildTableUI();
      return;
    }

    auto resList = res.toList();
    for (int i = 0; i < resList.length(); ++i) {
      auto item = resList[i];
      if (!item.isDict()) {
        continue;
      }

      auto dict = item.toDict();
      int scanId = static_cast<int>(dict["scan_id"].toLong());
      double theta = dict["theta"].toDouble();
      QString status = dict["status"].toString();
      bool unusable = (status == "fail" || status == "missing");

      ScanEntry entry;
      entry.scanId = scanId;
      entry.theta = theta;
      entry.status = status;
      entry.use = !unusable;
      scanEntries.append(entry);
    }

    rebuildTableUI();
  }

  void rebuildTableUI()
  {
    auto* table = ui.scanTable;
    table->clearContents();
    table->setRowCount(scanEntries.size());

    for (int i = 0; i < scanEntries.size(); ++i) {
      const auto& entry = scanEntries[i];
      bool unusable = (entry.status == "fail" || entry.status == "missing");

      auto* idItem = new QTableWidgetItem(QString::number(entry.scanId));
      idItem->setTextAlignment(Qt::AlignCenter);
      table->setItem(i, 0, idItem);

      auto* thetaItem = new QTableWidgetItem(
        unusable ? QString("-") : QString::number(entry.theta, 'f', 3));
      thetaItem->setTextAlignment(Qt::AlignCenter);
      table->setItem(i, 1, thetaItem);

      auto* statusItem = new QTableWidgetItem(entry.status);
      statusItem->setTextAlignment(Qt::AlignCenter);
      table->setItem(i, 2, statusItem);

      auto* cb = new QCheckBox(parent);
      cb->setChecked(entry.use);
      cb->setEnabled(!unusable);
      connect(cb, &QCheckBox::toggled, this, [this, i](bool b) {
        if (i < scanEntries.size()) {
          scanEntries[i].use = b;
        }
      });

      auto* tw = new QWidget(table);
      auto* layout = new QHBoxLayout(tw);
      layout->addWidget(cb);
      layout->setAlignment(Qt::AlignCenter);
      layout->setContentsMargins(0, 0, 0, 0);
      table->setCellWidget(i, 3, tw);
    }
  }

  // --- Filter SIDs ---

  void onLoadSidsFromTxt()
  {
    auto filePath = QFileDialog::getOpenFileName(
      parent.data(), "Select txt or csv file", workingDirectory(),
      "Text/CSV Files (*.txt *.csv)");
    if (filePath.isEmpty()) {
      return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
      qCritical() << "Failed to open file:" << filePath;
      return;
    }

    QTextStream reader(&file);

    if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
      loadSidsFromCsv(reader);
    } else {
      loadSidsFromTxt(reader);
    }
  }

  void loadSidsFromTxt(QTextStream& reader)
  {
    QStringList sids;
    while (!reader.atEnd()) {
      auto line = reader.readLine().trimmed();
      if (line.isEmpty() || line.startsWith('#')) {
        continue;
      }
      sids.append(line.split(' ')[0]);
    }

    ui.filterSidsString->setText(sids.join(", "));
  }

  void loadSidsFromCsv(QTextStream& reader)
  {
    // Read header to find column indices
    auto header = reader.readLine().trimmed();
    auto columns = header.split(',');
    for (auto& col : columns) {
      col = col.trimmed();
    }

    int sidCol = columns.indexOf("Scan ID");
    if (sidCol < 0) {
      sidCol = columns.indexOf("Scan_ID");
    }
    if (sidCol < 0) {
      qCritical() << "CSV file has no \"Scan ID\" column";
      return;
    }

    int useCol = columns.indexOf("Use");
    if (useCol < 0) {
      useCol = columns.indexOf("use");
    }

    QStringList sids;
    QList<int> sidInts;
    QList<bool> useFlags;
    while (!reader.atEnd()) {
      auto line = reader.readLine().trimmed();
      if (line.isEmpty() || line.startsWith('#')) {
        continue;
      }
      auto fields = line.split(',');
      if (sidCol >= fields.size()) {
        continue;
      }
      auto sidStr = fields[sidCol].trimmed();
      sids.append(sidStr);
      sidInts.append(sidStr.toInt());

      if (useCol >= 0 && useCol < fields.size()) {
        auto val = fields[useCol].trimmed();
        useFlags.append(val == "1" || val.toLower() == "x");
      }
    }

    ui.filterSidsString->setText(sids.join(", "));

    // If the CSV has a "Use" column, apply it to the scan table
    if (!useFlags.isEmpty()) {
      for (int i = 0; i < sidInts.size() && i < useFlags.size(); ++i) {
        for (auto& entry : scanEntries) {
          if (entry.scanId == sidInts[i]) {
            entry.use = useFlags[i];
            break;
          }
        }
      }
      rebuildTableUI();
    }
  }

  void applyFilter()
  {
    auto filterString = ui.filterSidsString->text().trimmed();
    if (filterString.isEmpty() || scanEntries.isEmpty()) {
      return;
    }

    importModule();

    Python python;
    auto func = pyxrfModule.findFunction("filter_sids");
    if (!func.isValid()) {
      qCritical() << "Failed to find tomviz.pyxrf.filter_sids";
      return;
    }

    QStringList allSids;
    for (const auto& entry : scanEntries) {
      if (entry.status != "fail" && entry.status != "missing") {
        allSids.append(QString::number(entry.scanId));
      }
    }

    Python::Dict kwargs;
    kwargs.set("all_sids", allSids);
    kwargs.set("filter_string", filterString);
    auto res = func.call(kwargs);

    if (!res.isValid() || !res.isList()) {
      qCritical() << "Error calling tomviz.pyxrf.filter_sids";
      return;
    }

    QSet<int> matchedIds;
    auto resList = res.toList();
    for (int i = 0; i < resList.length(); ++i) {
      matchedIds.insert(resList[i].toString().toInt());
    }

    for (int i = 0; i < scanEntries.size(); ++i) {
      auto& entry = scanEntries[i];
      if (entry.status == "fail" || entry.status == "missing") {
        continue;
      }
      entry.use = matchedIds.contains(entry.scanId);
    }

    rebuildTableUI();
  }

  void autoApplyFilter()
  {
    if (!ui.filterSidsString->text().trimmed().isEmpty() &&
        !scanEntries.isEmpty()) {
      applyFilter();
    }
  }

  // --- IC Names ---

  QStringList icNames()
  {
    QStringList ret;
    importModule();

    Python python;
    auto func = pyxrfModule.findFunction("ic_names");
    if (!func.isValid()) {
      return ret;
    }

    Python::Dict kwargs;
    kwargs.set("working_directory", workingDirectory());
    auto res = func.call(kwargs);

    if (!res.isValid()) {
      return ret;
    }

    for (auto& item : res.toVariant().toList()) {
      ret.append(item.toString().c_str());
    }
    return ret;
  }

  // --- PyXRF GUI ---

  void startPyXRFGUI()
  {
    if (pyxrfIsRunning) {
      return;
    }

    QString program = pyxrfGUICommand();
    auto environment = QProcessEnvironment::systemEnvironment();
    if (environment.contains("TOMVIZ_PYXRF_EXECUTABLE")) {
      program = environment.value("TOMVIZ_PYXRF_EXECUTABLE");
    }

    auto* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    process->start(program, QStringList());

    pyxrfIsRunning = true;

    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this]() { pyxrfIsRunning = false; });

    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError err) {
              pyxrfIsRunning = false;
              QString title;
              QString msg;
              if (err == QProcess::FailedToStart) {
                title = "PyXRF failed to start";
                msg = QString("The program \"%1\" failed to start.\n\n")
                        .arg(process->program()) +
                      "Try setting the environment variable "
                      "\"TOMVIZ_PYXRF_EXECUTABLE\" to the full path, "
                      "and restart tomviz.";
              } else {
                title = "PyXRF exited with an error";
                msg = process->readAllStandardError();
              }
              QMessageBox::critical(parent.data(), title, msg);
            });
  }

  // --- Validation ---

  void accepted()
  {
    QString reason;
    if (!validate(reason)) {
      QMessageBox::critical(parent.data(), "Invalid Settings", reason);
      parent->show();
      return;
    }

    writeSettings();
    parent->accept();
  }

  bool validate(QString& reason)
  {
    auto workingDir = workingDirectory();

    if (workingDir.isEmpty() || !QDir(workingDir).exists()) {
      reason = "Data directory does not exist: " + workingDir;
      return false;
    }

    if (scanRange().isEmpty()) {
      reason = "Scan range is required.";
      return false;
    }

    // Make paths absolute
    if (!QFileInfo(parametersFile()).isAbsolute() &&
        !parametersFile().isEmpty()) {
      setParametersFile(QDir(workingDir).filePath(parametersFile()));
    }

    if (parametersFile().isEmpty() || !QFile::exists(parametersFile())) {
      reason = "Parameters file does not exist: " + parametersFile();
      return false;
    }

    auto cmd = command();
    if (!executableExists(cmd)) {
      reason = QString("The pyxrf-utils executable \"%1\" was not found.")
                 .arg(cmd.isEmpty() ? QString("(empty)") : cmd);
      return false;
    }

    return true;
  }

  // --- Settings ---

  void readSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("pyxrf");

    auto savedCommand = settings->value("pyxrfUtilsCommand", "").toString();
    setCommand(findPyxrfUtilsCommand(savedCommand));

    setWorkingDirectory(
      settings->value("workingDirectory", defaultWorkingDirectory())
        .toString());
    setScanRange(settings->value("scanRange", "").toString());
    setRedownloadSuccessful(
      settings->value("redownloadSuccessful", false).toBool());
    ui.filterSidsString->setText(
      settings->value("filterSidsString", "").toString());

    settings->beginGroup("process");
    setPyxrfGUICommand(
      settings->value("pyxrfGUICommand", "pyxrf").toString());
    setParametersFile(settings->value("parametersFile", "").toString());
    setCsvOutput(settings->value("csvOutput", "").toString());
    setupComboBoxes();
    setIcName(settings->value("icName", "sclr1_ch4").toString());
    setSkipProcessed(settings->value("skipProcessed", true).toBool());
    setRotateDatasets(settings->value("rotateDatasets", true).toBool());
    settings->endGroup();

    settings->endGroup();

    populateScanTable();
    autoApplyFilter();
  }

  void writeSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("pyxrf");

    settings->setValue("pyxrfUtilsCommand", command());
    settings->setValue("workingDirectory", workingDirectory());
    settings->setValue("scanRange", scanRange());
    settings->setValue("redownloadSuccessful", redownloadSuccessful());
    settings->setValue("filterSidsString",
                       ui.filterSidsString->text().trimmed());

    settings->beginGroup("process");
    settings->setValue("pyxrfGUICommand", pyxrfGUICommand());
    settings->setValue("parametersFile", parametersFile());
    settings->setValue("csvOutput", csvOutput());
    settings->setValue("icName", icName());
    settings->setValue("skipProcessed", skipProcessed());
    settings->setValue("rotateDatasets", rotateDatasets());
    settings->endGroup();

    settings->endGroup();
  }

};

PyXRFDialog::PyXRFDialog(QWidget* parent)
  : QDialog(parent), m_internal(new Internal(this))
{
}

PyXRFDialog::~PyXRFDialog() = default;

void PyXRFDialog::show()
{
  m_internal->readSettings();
  QDialog::show();
}

QString PyXRFDialog::command() const
{
  return m_internal->command();
}

QString PyXRFDialog::workingDirectory() const
{
  return m_internal->workingDirectory();
}

QString PyXRFDialog::scanRange() const
{
  return m_internal->scanRange();
}

QString PyXRFDialog::skipScanIds() const
{
  return m_internal->skipScanIds();
}

bool PyXRFDialog::redownloadSuccessful() const
{
  return m_internal->redownloadSuccessful();
}

QString PyXRFDialog::parametersFile() const
{
  return m_internal->parametersFile();
}

QString PyXRFDialog::icName() const
{
  return m_internal->icName();
}

bool PyXRFDialog::skipProcessed() const
{
  return m_internal->skipProcessed();
}

bool PyXRFDialog::rotateDatasets() const
{
  return m_internal->rotateDatasets();
}

QString PyXRFDialog::csvOutput() const
{
  return m_internal->csvOutput();
}

} // namespace tomviz
