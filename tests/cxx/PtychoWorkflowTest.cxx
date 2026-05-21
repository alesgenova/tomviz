/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTest>

#include <pqPVApplicationCore.h>

#include "PythonUtilities.h"
#include "Utilities.h"

#include "pipeline/OutputPort.h"
#include "pipeline/sources/PythonSource.h"

#include "TomvizTest.h"

using namespace tomviz;
using namespace tomviz::pipeline;

const QDir ROOT_DATA_DIR = QString(SOURCE_DIR) + "/data";
const QDir DATA_DIR = ROOT_DATA_DIR.absolutePath() + "/Pt_Zn_Phase";

class PtychoWorkflowTest : public QObject
{
  Q_OBJECT

private:
  void downloadDataIfMissing()
  {
    if (DATA_DIR.exists()) {
      return;
    }
    QString python = "python";
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (env.contains("TOMVIZ_TEST_PYTHON_EXECUTABLE")) {
      python = env.value("TOMVIZ_TEST_PYTHON_EXECUTABLE");
    }

    auto scriptFile =
      QFileInfo(QString(SOURCE_DIR) + "/fixtures/download_and_unzip.py");
    QString scriptPath = scriptFile.absoluteFilePath();

    QString url =
      "https://data.kitware.com/api/v1/file/6914aad883abdcd84d150c91/download";

    QStringList arguments;
    arguments << scriptPath << url << ROOT_DATA_DIR.absolutePath();

    QProcess process;
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(python, arguments);

    int timeout = 60 * 10;
    QVERIFY(process.waitForFinished(timeout * 1000));
    QVERIFY(process.exitCode() == 0);
  }

private slots:
  void initTestCase() { downloadDataIfMissing(); }

  void cleanupTestCase() {}

  void runTest()
  {
    QString ptychoDir = DATA_DIR.absolutePath() + "/Ptycho/recon_result/";
    QString outputDir = DATA_DIR.absolutePath() + "/output/";

    QString outputInfoFile = outputDir + "stacked_ptycho_info.txt";

    QDir outDir(outputDir);
    outDir.mkpath(".");
    QFile::remove(outputInfoFile);

    auto jsonDesc = readInJSONDescription("PtychoSource");
    QVERIFY2(!jsonDesc.contains("raise IOError"),
             "Could not read PtychoSource.json");

    auto script = readInPythonScript("PtychoSource");
    QVERIFY2(!script.contains("raise IOError"),
             "Could not read PtychoSource.py");

    auto* source = new PythonSource(this);
    source->setJSONDescription(jsonDesc);
    source->setScript(script);

    source->setParameter("ptycho_dir", ptychoDir);
    source->setParameter("output_info_file", outputInfoFile);
    source->setParameter("rotate_datasets", true);
    source->setParameter("sid_list", "[157391,157394,157397]");
    source->setParameter("version_list", R"(["t1","t1","t1"])");
    source->setParameter("angle_list", "[-90.0,-89.0,-88.0]");

    bool ok = source->execute();
    QVERIFY2(ok, "PtychoSource::execute() failed");

    auto* objectPort = source->outputPort("object");
    QVERIFY(objectPort);
    QVERIFY2(objectPort->hasData(), "object output port has no data");

    auto* probePort = source->outputPort("probe");
    QVERIFY(probePort);
    QVERIFY2(probePort->hasData(), "probe output port has no data");

    QVERIFY2(QFile::exists(outputInfoFile),
             "Info text file was not written");
  }
};

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  pqPVApplicationCore appCore(argc, argv);

  Python::initialize();

  PtychoWorkflowTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "PtychoWorkflowTest.moc"
