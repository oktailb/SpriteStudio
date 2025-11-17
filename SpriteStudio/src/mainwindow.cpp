#include "include/mainwindow.h"
#include "./ui_mainwindow.h"
#include "qfiledialog.h"
#include "ui_mainwindow.h"
#include <charconv>
#include <stdint.h>
#include <stdfloat>
#include <netinet/in.h>
#include <QtGui>
#include <QMessageBox>

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
    QFileDialog f;;
    f.show();
}

void MainWindow::on_actionExit_triggered()
{
    exit(EXIT_SUCCESS);
}

