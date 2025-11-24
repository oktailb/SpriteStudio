#include "aboutdialog.h"
#include "generated/version.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QScreen>
#include <QGuiApplication>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
  setupUI();
  loadApplicationInfo();
  loadCredits();
  loadLicense();

  // Configuration de la fenêtre
  setWindowTitle(tr("_about") + "Sprite Studio");
  setMinimumSize(500, 400);
  resize(600, 500);

  // Centrer la fenêtre
  QScreen *screen = QGuiApplication::primaryScreen();
  QRect screenGeometry = screen->geometry();
  move(screenGeometry.center() - rect().center());
}

void AboutDialog::setupUI()
{
  // Widget principal
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // En-tête avec icône et titre
  QHBoxLayout *headerLayout = new QHBoxLayout();

  // Icône de l'application (vous pouvez remplacer par votre propre icône)
  iconLabel = new QLabel();
  QPixmap appIcon(":/drawer/play.png"); // Utilise une icône existante ou créez-en une
  if (!appIcon.isNull()) {
      iconLabel->setPixmap(appIcon.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
  iconLabel->setAlignment(Qt::AlignCenter);

  // Titre et version
  QVBoxLayout *titleLayout = new QVBoxLayout();
  titleLabel = new QLabel("Sprite Studio");
  titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #2c3e50;");

  versionLabel = new QLabel("Version " + QString(PROJECT_VERSION));
  versionLabel->setStyleSheet("font-size: 14px; color: #7f8c8d;");

  titleLayout->addWidget(titleLabel);
  titleLayout->addWidget(versionLabel);
  titleLayout->setAlignment(Qt::AlignCenter);

  headerLayout->addWidget(iconLabel);
  headerLayout->addLayout(titleLayout);
  headerLayout->setAlignment(Qt::AlignCenter);

  mainLayout->addLayout(headerLayout);

  // Ligne séparatrice
  QFrame *line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  line->setStyleSheet("color: #bdc3c7;");
  mainLayout->addWidget(line);

  // Onglets
  tabWidget = new QTabWidget();

  // Onglet "About"
  aboutText = new QTextEdit();
  aboutText->setReadOnly(true);
  aboutText->setStyleSheet("QTextEdit { background: transparent; border: none; }");
  tabWidget->addTab(aboutText, tr("_about"));

  // Onglet "Credits"
  creditsText = new QTextEdit();
  creditsText->setReadOnly(true);
  creditsText->setStyleSheet("QTextEdit { background: transparent; border: none; }");
  tabWidget->addTab(creditsText, tr("_credits"));

  // Onglet "License"
  licenseText = new QTextEdit();
  licenseText->setReadOnly(true);
  licenseText->setStyleSheet("QTextEdit { background: transparent; border: none; }");
  tabWidget->addTab(licenseText, tr("_licence"));

  mainLayout->addWidget(tabWidget);

  // Bouton Fermer
  closeButton = new QPushButton(tr("_close"));
  closeButton->setStyleSheet(
      "QPushButton {"
      "    background: #3498db;"
      "    color: white;"
      "    border: none;"
      "    padding: 8px 16px;"
      "    border-radius: 4px;"
      "    font-weight: bold;"
      "}"
      "QPushButton:hover {"
      "    background: #2980b9;"
      "}"
      "QPushButton:pressed {"
      "    background: #21618c;"
      "}"
      );
  connect(closeButton, &QPushButton::clicked, this, &AboutDialog::accept);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();
  buttonLayout->addWidget(closeButton);
  buttonLayout->addStretch();

  mainLayout->addLayout(buttonLayout);

  // Style général de la fenêtre
  setStyleSheet(
      "AboutDialog {"
      "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ecf0f1, stop:1 #bdc3c7);"
      "}"
      "QTabWidget::pane {"
      "    border: 1px solid #bdc3c7;"
      "    background: white;"
      "}"
      "QTabWidget::tab-bar {"
      "    alignment: center;"
      "}"
      "QTabBar::tab {"
      "    background: #ecf0f1;"
      "    border: 1px solid #bdc3c7;"
      "    padding: 8px 16px;"
      "    margin: 2px;"
      "}"
      "QTabBar::tab:selected {"
      "    background: #3498db;"
      "    color: white;"
      "}"
      "QTabBar::tab:hover {"
      "    background: #d5dbdb;"
      "}"
      );
}

void AboutDialog::loadApplicationInfo()
{
  QString aboutHtml = QString(
                          "<div style='text-align: center; margin: 20px;'>"
                          "<h2 style='color: #2c3e50;'>Sprite Studio</h2>"
                          "<p style='color: #7f8c8d; font-size: 14px;'>"
                          + tr("_purpose") +
                          "</p>"
                          "<hr>"
                          "<div style='text-align: left; margin: 20px;'>"
                          "<p><b>Tipeee:</b> https://en.tipeee.com/lecoq-vincent</p>"
                          "<p><b>Github:</b> https://github.com/oktailb/SpriteStudio</p>"
                          "</div>"
                          "<hr>"
                          "<div style='text-align: left; margin: 20px;'>"
                          "<p><b>Version:</b> %1</p>"
                          "<p><b>Build:</b> %2</p>"
                          "<p><b>" + tr("_build_with") + ": </b> Qt %3 %4 %5 %6</p>"
                          "<p><b>" + tr("_platform") + ": </b> %7</p>"
                          "</div>"
                          "<div style='text-align: left; background: #f8f9fa; padding: 15px; border-radius: 8px; margin: 15px;'>"
                          "<h4 style='color: #2c3e50; margin-top: 0;'>" + tr("_git_info") + "</h4>"
                          "<p style='margin: 5px 0;'><b>" + tr("_branch") + ": </b> %8</p>"
                          "<p style='margin: 5px 0;'><b>" + tr("_commit") + ": </b> %9</p>"
                          "<p style='margin: 5px 0;'><b>" + tr("_last_commit_date") + ": </b> %10</p>"
                          "<p style='margin: 5px 0;'><b>" + tr("_last_author") + ": </b> %11</p>"
                          "</div>"
                          "</div>"
                          )
                          .arg(PROJECT_VERSION)
                          .arg(PROJECT_BUILD_DATE)
                          .arg(QT_VERSION_STR)
                          .arg(CMAKE_CXX_COMPILER_ID)
                          .arg(CMAKE_CXX_COMPILER)
                          .arg(CMAKE_CXX_COMPILER_VERSION)
                          .arg(QSysInfo::prettyProductName())
                          .arg(GIT_BRANCH)
                          .arg(GIT_COMMIT_HASH)
                          .arg(GIT_COMMIT_DATE)
                          .arg(GIT_LAST_AUTHOR);

  aboutText->setHtml(aboutHtml);
}

void AboutDialog::loadCredits()
{
  QString creditsHtml = QString(
                            "<div style='text-align: center;margin: 20px;'>"
                            "<h3 style='color: #2c3e50;'>" + tr("_credits_and_greetings") + "</h3>"
                            "<hr>"
                            "<div style='text-align: left; margin: 20px;'>"
                            "<p><b>Tipeee:</b> https://en.tipeee.com/lecoq-vincent</p>"
                            "<p><b>Github:</b> https://github.com/oktailb/SpriteStudio</p>"
                            "</div>"
                            "<hr>"
                            "<div style='text-align: left; background: #f8f9fa; padding: 15px; border-radius: 8px; margin: 15px;'>"
                            "<h4>" + tr("_contributors") + "</h4>"
                            "<div style='background: #f8f9fa; padding: 15px; border-radius: 8px;'>"
                            "<p style='margin: 0;'>%1</p>"
                            "</div>"
                            "<h4 style='margin-top: 20px;'>" + tr("_used_techno") + "</h4>"
                            "<ul>"
                            "<li><b>Qt Framework:</b> Version %2 https://www.qt.io</li>"
                            "<li><b>" + tr("_compiler") + ":</b> %3 %4 %5</li>"
                            "<li><b>CMake:</b> %6</li>"
                            "</ul>"
                            "<h4>" + tr("_greetings_title") + "</h4>"
                            "<p>" + tr("_greetings") + "</p>"
                            "</div>"
                            "</div>"
                            )
                            .arg(GIT_AUTHORS)
                            .arg(QT_VERSION_STR)
                            .arg(CMAKE_CXX_COMPILER_ID)
                            .arg(CMAKE_CXX_COMPILER)
                            .arg(CMAKE_CXX_COMPILER_VERSION)
                            .arg(CMAKE_VERSION);

  creditsText->setHtml(creditsHtml);
}

QString AboutDialog::readTextFile(const QString &filePath)
{
  QFile file(filePath);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qWarning() << "Impossible d'ouvrir le fichier de ressource de licence:" << filePath;
      return QString("Erreur: Fichier de licence non trouvé dans les ressources: ") + filePath;
    }

  QTextStream in(&file);
  return in.readAll();
}

void AboutDialog::loadLicense()
{
  // Lire le fichier de licence comme avant
  QString licenseTextContent = readTextFile(":/text/license.txt");

  QString licenseHtml = QString(
                            "<div style='margin: 20px; font-family: monospace; font-size: 12px;'>"
                            "<h3 style='color: #2c3e50;'>" + tr("_licence_agreement") + "</h3>"
                            "<hr>"
                            "<pre>%1</pre>"
                            "</div>"
                            ).arg(licenseTextContent.toHtmlEscaped());

  licenseText->setHtml(licenseHtml);
}
