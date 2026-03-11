#include <QApplication>

#include "MainWindow.hpp"

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  app.setApplicationName("Video Preview Generator");
  app.setOrganizationName("Video Preview Generator");

  MainWindow window;
  window.show();

  return app.exec();
}
