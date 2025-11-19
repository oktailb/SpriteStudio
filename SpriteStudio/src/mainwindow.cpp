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
    , frameModel(new ArrangementModel(this))
    , timerId(0)
    , ready(false)
{
    ui->setupUi(this);
    timerId = startTimer(100);
    ready = true;
    ui->framesList->setModel(frameModel);
    ui->framesList->setViewMode(QListView::IconMode);
    ui->timingLabel->setText(" -> Timing " + QString::number(1000.0  / (double)ui->fps->value(), 'g', 4) + "ms");
}

void MainWindow::populateFrameList(const QList<QPixmap> &frameList, const QList<Extractor::Box> &boxList)
{
    frameModel->clear();
    frameBoxes.clear();

    frameModel->setColumnCount(1);

    for (int i = 0; i < frameList.size(); ++i) {
        const QPixmap &pixmap = frameList.at(i);
        const Extractor::Box &box = boxList.at(i);

        QStandardItem *item = new QStandardItem();

        QPixmap thumbnail = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        item->setData(thumbnail, Qt::DecorationRole);
        item->setData(QString("Frame %1").arg(i + 1), Qt::DisplayRole);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        frameModel->appendRow(item);
        frameBoxes.append(box);
    }
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

    frames.clear();
    frame = QPixmap();
    QList<Extractor::Box> boxes;

    if (type == "gif") {
        GifExtractor extractor;
        frames = extractor.extractFrames(fileName);
        frame = extractor.m_atlas;
        boxes = extractor.m_atlas_index;
    }
    else if ((type == "png") || (type == "jpg") ||  (type == "bmp") || (type == "gif")) {
        SpriteExtractor extractor;
        frames = extractor.extractFrames(fileName);
        frame = extractor.m_atlas;
        boxes = extractor.m_atlas_index;
    } else if (type == "json") {

    } else {
        QMessageBox::warning(this, "Erreur d'ouverture", "Format de fichier non supporté.");
        return;
    }

    if (!frames.isEmpty()) {
        auto view = ui->graphicsViewLayers;
        auto scene = new QGraphicsScene(view);

        QGraphicsPixmapItem *item = new QGraphicsPixmapItem(frame);
        scene->addItem(item);
        item->setPos(0, 0);
        view->setScene(scene);
        view->show();

        populateFrameList(frames, boxes);
    }
}

void MainWindow::on_actionExit_triggered()
{
    exit(EXIT_SUCCESS);
}

void MainWindow::on_fps_valueChanged(int fps)
{
    ui->timingLabel->setText(" -> Timing " + QString::number(1000.0  / (double)fps, 'g', 4) + "ms");
}
