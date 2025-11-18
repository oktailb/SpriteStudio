#include "include/mainwindow.h"
#include "./ui_mainwindow.h"
#include "qfiledialog.h"
#include "ui_mainwindow.h"
#include <stdfloat>
#include <netinet/in.h>
#include <QtGui>
#include <QDialog>
#include <QTextEdit>
#include <QMessageBox>
#include <QGraphicsPixmapItem>
#include "gifextractor.h"
#include "spriteextractor.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
      , ui(new Ui::MainWindow)
      , timerId(0)
      , ready(false)
{
  ui->setupUi(this);
  timerId = startTimer(100);
  ready = true;
}

void MainWindow::closeEvent(QCloseEvent *e)
{
  Q_UNUSED(e);
}

MainWindow::~MainWindow()
{
  killTimer(timerId);
  delete ui;
}

void MainWindow::timerEvent(QTimerEvent *event)
{
  Q_UNUSED(event);
}

QString readTextFile(const QString &filePath)
{
  QFile file(filePath);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qWarning() << "Impossible d'ouvrir le fichier de ressource de licence:" << filePath;
      return QString("Erreur: Fichier de licence non trouvé dans les ressources: ") + filePath;
    }

  QTextStream in(&file);
  return in.readAll();
}

void MainWindow::on_actionLicence_triggered()
{
  QString licenseText = readTextFile(":/text/license.txt");
  QDialog dialog;
  dialog.setWindowTitle("Licence");
  dialog.setMinimumSize(600, 400);
  QTextEdit *textEdit = new QTextEdit(&dialog);
  textEdit->setPlainText(licenseText);
  textEdit->setReadOnly(true);
  QPushButton *closeButton = new QPushButton("Close", &dialog);
  QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->addWidget(textEdit);
  layout->addWidget(closeButton);
  dialog.exec();
}

void MainWindow::on_actionAbout_triggered()
{
  QString aboutText = readTextFile(":/text/about.txt");
  QDialog dialog;
  dialog.setWindowTitle("About");
  dialog.setMinimumSize(600, 400);
  QTextEdit *textEdit = new QTextEdit(&dialog);
  textEdit->setPlainText(aboutText);
  textEdit->setReadOnly(true);
  QPushButton *closeButton = new QPushButton("Close", &dialog);
  QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->addWidget(textEdit);
  layout->addWidget(closeButton);
  dialog.exec();
}

void MainWindow::on_actionOpen_triggered()
{
  QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("Images (*.png *.jpg *.bmp *.gif *.json)"));
  QString type = fileName.split(".").last();
  if (type == "gif") {
      GifExtractor extractor;
      frames = extractor.extractFrames(fileName);
    }
  else if ((type == "png") || (type == "jpg") ||  (type == "bmp") || (type == "gif")) {
      SpriteExtractor extractor;
      frames = extractor.extractFrames(fileName);
    } else if (type == "json") {

    } else {
      throw;
    }
  if (!frames.isEmpty()) {
      auto view = ui->graphicsViewLayers;
      auto scene = new QGraphicsScene(view);
      for (int i = 0; i < frames.size(); ++i) {
          const QPixmap &frame = frames.at(i);

          QGraphicsPixmapItem *item = new QGraphicsPixmapItem(frame);
          scene->addItem(item);
          item->setPos(0, i * frame.height());
        }
      view->setScene(scene);
      view->show();
    }
}

void MainWindow::on_actionExit_triggered()
{
  exit(EXIT_SUCCESS);
}

