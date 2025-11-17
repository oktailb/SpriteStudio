#include "include/mainwindow.h"
#include "./ui_mainwindow.h"
#include "qfiledialog.h"
#include "ui_mainwindow.h"
#include <stdfloat>
#include <netinet/in.h>
#include <QtGui>
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

void MainWindow::on_actionLicence_triggered()
{

}

void MainWindow::on_actionAbout_triggered()
{

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

