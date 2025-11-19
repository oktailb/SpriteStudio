#include "include/mainwindow.h"
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  QString locale = QLocale::system().name();
  QTranslator translator;
  if (translator.load("sprite_studio_" + locale, QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    {
    } else if (translator.load("sprite_studio_" + locale, a.applicationDirPath() + "/i18n")) {
    }
  a.installTranslator(&translator);

  MainWindow w;

  w.show();

  return a.exec();
}
