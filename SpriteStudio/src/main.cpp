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

  qDebug() << "Locale is " << locale;
  qDebug() << "App PATH is " << a.applicationDirPath();
  qDebug() << "Qt PATH is " << QLibraryInfo::path(QLibraryInfo::TranslationsPath);
  if (translator.load("sprite_studio_" + locale, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
      qDebug() << "Loaded from system translations";
    } else if (translator.load("sprite_studio_" + locale, a.applicationDirPath() + "/i18n")) {
      qDebug() << "Loaded from app dir";
    } else if (translator.load("sprite_studio_" + locale, ":/i18n/")) {
      qDebug() << "Loaded from embeed resource";
    } else {
      qDebug() << "Failed to load translation for " << locale;
    }
  a.installTranslator(&translator);

  MainWindow w;

  w.show();

  return a.exec();
}
