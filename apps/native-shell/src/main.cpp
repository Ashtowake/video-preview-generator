#include <clocale>

#include <QApplication>
#include <QCoreApplication>
#include <QLocale>
#include <QSurfaceFormat>
#include <QByteArray>

#include "MainWindow.hpp"

int main(int argc, char *argv[])
{
  std::setlocale(LC_NUMERIC, "C");
  qputenv("LC_NUMERIC", QByteArray("C"));

  QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setProfile(QSurfaceFormat::CompatibilityProfile);
  format.setVersion(2, 1);
  format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  QSurfaceFormat::setDefaultFormat(format);

  QApplication app(argc, argv);
  std::setlocale(LC_NUMERIC, "C");
  qputenv("LC_NUMERIC", QByteArray("C"));
  QLocale::setDefault(QLocale::system());
  app.setApplicationName("Video Preview Generator");
  app.setOrganizationName("Video Preview Generator");

  MainWindow window;
  window.show();

  return app.exec();
}
