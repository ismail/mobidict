#include <QApplication>

#include "mainwindow.h"

int main(int argc, char **argv)
{
  QCoreApplication::setOrganizationName("i10z");
  QCoreApplication::setOrganizationDomain("i10z.com");
  QCoreApplication::setApplicationName("mobidict");

  QApplication app(argc, argv);
  MainWindow m;

  m.discoverDictionaries();
  m.show();

  return app.exec();
}
