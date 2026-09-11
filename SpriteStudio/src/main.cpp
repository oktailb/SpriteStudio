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
  bool loaded = false;

  if (translator.load("sprite_studio_" + locale, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
      loaded = true;
  } else if (translator.load("sprite_studio_" + locale, a.applicationDirPath() + "/i18n")) {
      loaded = true;
  } else if (translator.load("sprite_studio_" + locale, ":/i18n/")) {
      loaded = true;
  }

  // Fallback to English catalog if system translation was not found
  if (!loaded) {
      if (translator.load("sprite_studio_en_US", ":/i18n/") ||
          translator.load("sprite_studio_en_US", a.applicationDirPath() + "/i18n")) {
          loaded = true;
      }
  }

  if (loaded) {
      a.installTranslator(&translator);
  }

  MainWindow w;

  if (argc > 1) {
      w.processFile(QString::fromLocal8Bit(argv[1]));
  }

  w.show();

  return a.exec();
}
