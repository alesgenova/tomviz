/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

#include <pqPVApplicationCore.h>

#include "PythonUtilities.h"
#include "Utilities.h"

#include "pipeline/OutputPort.h"
#include "pipeline/sources/PythonSource.h"

#include "TomvizTest.h"

using namespace tomviz;
using namespace tomviz::pipeline;

class PyXRFWorkflowTest : public QObject
{
  Q_OBJECT

private:
  QTemporaryDir m_tempDir;

  bool createSyntheticTomoH5(const QString& outputPath)
  {
    QString python = "python";
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (env.contains("TOMVIZ_TEST_PYTHON_EXECUTABLE")) {
      python = env.value("TOMVIZ_TEST_PYTHON_EXECUTABLE");
    }

    auto scriptFile =
      QFileInfo(QString(SOURCE_DIR) + "/fixtures/create_synthetic_tomo.py");
    QString scriptPath = scriptFile.absoluteFilePath();

    QDir().mkpath(QFileInfo(outputPath).absolutePath());

    QProcess process;
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(python, { scriptPath, outputPath });

    if (!process.waitForFinished(30000)) {
      qCritical() << "Timed out creating synthetic tomo.h5";
      return false;
    }

    return process.exitCode() == 0;
  }

private slots:
  void initTestCase()
  {
    QVERIFY2(m_tempDir.isValid(), "Failed to create temp directory");
  }

  void cleanupTestCase() {}

  void runTest()
  {
    QString outputDir = m_tempDir.path() + "/recon";
    QString tomoFile = outputDir + "/tomo.h5";

    bool created = createSyntheticTomoH5(tomoFile);
    QVERIFY2(created, "Failed to create synthetic tomo.h5");
    QVERIFY2(QFile::exists(tomoFile), "Synthetic tomo.h5 does not exist");

    auto jsonDesc = readInJSONDescription("PyXRFSource");
    QVERIFY2(!jsonDesc.contains("raise IOError"),
             "Could not read PyXRFSource.json");

    auto script = readInPythonScript("PyXRFSource");
    QVERIFY2(!script.contains("raise IOError"),
             "Could not read PyXRFSource.py");

    auto* source = new PythonSource(this);
    source->setJSONDescription(jsonDesc);
    source->setScript(script);

    // Empty scan_range skips all subprocess calls and goes straight
    // to reading tomo.h5 from the output directory.
    source->setParameter("working_directory", outputDir);
    source->setParameter("scan_range", QString(""));
    source->setParameter("skip_scan_ids", QString("[]"));
    source->setParameter("redownload_successful", false);
    source->setParameter("skip_processed", true);
    source->setParameter("rotate_datasets", false);
    source->setParameter("pyxrf_utils_command", QString("pyxrf-utils"));
    source->setParameter("parameters_file", QString(""));
    source->setParameter("ic_name", QString("sclr1_ch4"));
    source->setParameter("csv_output", QString(""));

    bool ok = source->execute();
    QVERIFY2(ok, "PyXRFSource::execute() failed");

    auto* elementsPort = source->outputPort("elements");
    QVERIFY(elementsPort);
    QVERIFY2(elementsPort->hasData(), "elements output port has no data");
  }
};

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  pqPVApplicationCore appCore(argc, argv);

  Python::initialize();

  PyXRFWorkflowTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "PyXRFWorkflowTest.moc"
