/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>

#include <QSplashScreen>
#include <QSurfaceFormat>

#include <QDebug>

#include <pqPVApplicationCore.h>

#include <QVTKOpenGLStereoWidget.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

#include "loguru.hpp"
#include "MainWindow.h"
#include "PythonUtilities.h"
#include "tomvizConfig.h"
#include "tomvizPythonConfig.h"

#include <clocale>

#if __has_include(<viskores/cont/Initialize.h>)
#include <viskores/cont/Initialize.h>
#define TOMVIZ_HAS_VISKORES
#endif

int main(int argc, char** argv)
{
  // Set up loguru, for printing stack traces on crashes
  loguru::g_stderr_verbosity = loguru::Verbosity_ERROR;
  loguru::init(argc, argv);

#ifdef TOMVIZ_HAS_VISKORES
  // Initialize Viskores (VTK-m) runtime before any VTK filters use it.
  viskores::cont::Initialize();
#endif

#ifdef Q_OS_LINUX
  // VTK's render windows use X11/GLX, which conflicts with the Qt Wayland
  // platform plugin and causes BadAccess on glXMakeCurrent the second time a
  // render view is created. Force xcb on Wayland sessions unless the user has
  // already chosen a platform.
  if (qgetenv("XDG_SESSION_TYPE") == "wayland" &&
      !qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", "xcb");
  }
#endif

  QSurfaceFormat::setDefaultFormat(QVTKOpenGLStereoWidget::defaultFormat());

  QCoreApplication::setApplicationName("tomviz");
  QCoreApplication::setApplicationVersion(TOMVIZ_VERSION);
  QCoreApplication::setOrganizationName("tomviz");

  tomviz::InitializePythonEnvironment(argc, argv);

  QApplication app(argc, argv);

  QPixmap pixmap(":/icons/tomvizfull.png");
  QSplashScreen splash(pixmap);
  splash.show();
  app.processEvents();

  std::string exeDir = QApplication::applicationDirPath().toLatin1().data();
  if (tomviz::isApplicationBundle(exeDir)) {
    QByteArray pythonPath = tomviz::bundlePythonPath(exeDir).c_str();
    qputenv("PYTHONPATH", pythonPath);
    qputenv("PYTHONHOME", pythonPath);
  }

  // Set environment variable to indicate that we are running inside the Tomviz
  // application vs Python command line. This can be used to selectively load
  // modules.
  qputenv("TOMVIZ_APPLICATION", "1");

  // If we don't initialize Python here, the application freezes when exiting
  // at the very end, during Py_Finalize().
  tomviz::Python::initialize();

  setlocale(LC_NUMERIC, "C");
  pqPVApplicationCore appCore(argc, argv);
  tomviz::MainWindow window;
  window.show();
  splash.finish(&window);
  window.openFiles(argc, argv);

  return app.exec();
}
