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
#include <QGraphicsScene>
#include <QPen>
#include <QGraphicsRectItem>
#include "gifextractor.h"
#include "spriteextractor.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , frameModel(new ArrangementModel(this))
    , timerId(0)
    , ready(false)
    , boundingBoxHighlighter(nullptr)
{
    ui->setupUi(this);
    setAcceptDrops(true);
    timerId = startTimer(100);
    ready = true;
    ui->framesList->setModel(frameModel);
    ui->framesList->setViewMode(QListView::IconMode);
    ui->timingLabel->setText(" -> Timing " + QString::number(1000.0  / (double)ui->fps->value(), 'g', 4) + "ms");
    QObject::connect(ui->framesList, &QListView::clicked,
                     this, &MainWindow::on_framesList_clicked);
    QObject::connect(frameModel, &ArrangementModel::mergeRequested,
                     this, &MainWindow::onMergeFrames);
    listDelegate = new FrameDelegate(this);
    ui->framesList->setItemDelegate(listDelegate);
    ui->framesList->viewport()->installEventFilter(this);
}

void MainWindow::setMergeHighlight(const QModelIndex &index, bool show)
{
    // Nettoyage préventif
    if (!show || !index.isValid()) {
        clearMergeHighlight();
        return;
    }

    int row = index.row();
    if (row >= 0 && row < frameBoxes.size()) {
        // On récupère la box correspondante
        Extractor::Box box = frameBoxes[row];
        QRectF rect(box.x, box.y, box.w, box.h);

        // Création lazy du rectangle
        if (!mergeHighlighter) {
            mergeHighlighter = new QGraphicsRectItem();
            QPen pen(Qt::magenta);
            pen.setWidth(3);
            pen.setStyle(Qt::DotLine);
            mergeHighlighter->setPen(pen);
            // Important : ZValue élevé pour être au-dessus de l'image
            mergeHighlighter->setZValue(10);

            if (ui->graphicsViewLayers->scene()) {
                ui->graphicsViewLayers->scene()->addItem(mergeHighlighter);
            }
        }

        // Si la scène a changé entre temps (rechargement de fichier)
        if (mergeHighlighter->scene() != ui->graphicsViewLayers->scene()) {
            if (ui->graphicsViewLayers->scene()) {
                ui->graphicsViewLayers->scene()->addItem(mergeHighlighter);
            }
        }

        mergeHighlighter->setRect(rect);
        mergeHighlighter->setVisible(true);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->framesList->viewport()) {

        if (event->type() == QEvent::DragMove) {
            QDragMoveEvent *dmEvent = static_cast<QDragMoveEvent *>(event);
            QModelIndex index = ui->framesList->indexAt(dmEvent->pos());

            if (index.isValid()) {
                // === CAS FUSION (Souris SUR un item) ===

                // 1. Visuel QListView : On dit au délégué de dessiner le cadre Magenta sur cet item
                listDelegate->setMergeTarget(index.row());

                // 2. Visuel QListView : On cache la barre d'insertion noire (trait entre les items)
                ui->framesList->setDropIndicatorShown(false);

                // 3. Visuel Atlas (votre code existant)
                setMergeHighlight(index, true);
            }
            else {
                // === CAS INSERTION (Souris ENTRE deux items) ===

                // 1. Visuel QListView : On dit au délégué de ne rien dessiner de spécial
                listDelegate->setMergeTarget(-1);

                // 2. Visuel QListView : On AFFICHE la barre d'insertion noire standard
                // (C'est le visuel standard "Insertion" de Qt, très clair pour l'utilisateur)
                ui->framesList->setDropIndicatorShown(true);

                // 3. Visuel Atlas : On efface le rectangle
                clearMergeHighlight();
            }

            // IMPORTANT : Forcer la liste à se redessiner immédiatement pour voir les changements
            ui->framesList->viewport()->update();
        }
        else if (event->type() == QEvent::DragLeave || event->type() == QEvent::Drop) {
            // Nettoyage quand on quitte
            listDelegate->setMergeTarget(-1);
            ui->framesList->setDropIndicatorShown(true);
            clearMergeHighlight();
            ui->framesList->viewport()->update();
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::clearMergeHighlight()
{
    if (mergeHighlighter) {
        mergeHighlighter->setVisible(false);
    }
}

void MainWindow::onMergeFrames(int sourceRow, int targetRow)
{
    if (sourceRow < 0 || sourceRow >= frameBoxes.size() ||
        targetRow < 0 || targetRow >= frameBoxes.size()) {
        return;
    }

    // 1. Calcul et Fusion (Identique à avant)
    Extractor::Box srcBox = frameBoxes[sourceRow];
    Extractor::Box tgtBox = frameBoxes[targetRow];

    QRect srcRect(srcBox.x, srcBox.y, srcBox.w, srcBox.h);
    QRect tgtRect(tgtBox.x, tgtBox.y, tgtBox.w, tgtBox.h);
    QRect unitedRect = srcRect.united(tgtRect);

    QPixmap mergedPixmap = this->frame.copy(unitedRect);

    // 2. Mise à jour des données internes de la CIBLE
    Extractor::Box newBox;
    newBox.x = unitedRect.x();
    newBox.y = unitedRect.y();
    newBox.w = unitedRect.width();
    newBox.h = unitedRect.height();

    frameBoxes[targetRow] = newBox;
    frames[targetRow] = mergedPixmap;

    // 3. Mise à jour VISUELLE de la CIBLE uniquement
    QStandardItem *targetItem = frameModel->item(targetRow);
    if (targetItem) {
        QPixmap thumbnail = mergedPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        targetItem->setData(thumbnail, Qt::DecorationRole);
        // Mise à jour du texte pour refléter les nouvelles coordonnées
        targetItem->setData(QString("Merged\n[%1,%2](%3x%4)")
                                .arg(newBox.x).arg(newBox.y).arg(newBox.w).arg(newBox.h),
                            Qt::DisplayRole);
    }

    // 4. SUPPRESSION DE LA SOURCE
    // ATTENTION : On supprime uniquement des listes de données internes !
    // On ne touche PAS au frameModel->removeRow(sourceRow) ici.
    // Le mode InternalMove de la QListView le fera automatiquement au retour du drop.

    frameBoxes.removeAt(sourceRow);
    frames.removeAt(sourceRow);

    // Nettoyage de la vue graphique
    if (boundingBoxHighlighter) {
        ui->graphicsViewLayers->scene()->removeItem(boundingBoxHighlighter);
        delete boundingBoxHighlighter;
        boundingBoxHighlighter = nullptr;
    }

    qDebug() << "Fusion terminée. Model laissé au soin de Qt InternalMove.";
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
        item->setData(QString("Frame %1\n[%2,%3](%4x%5)").arg(i + 1).arg(box.x).arg(box.y).arg(box.w).arg(box.h), Qt::DisplayRole);
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
    currentFilePath = fileName;
    processFile(currentFilePath);
}

void MainWindow::processFile(const QString &fileName)
{
    int alphaThreshold = ui->alphaThreshold->value();
    int verticalTolerance = ui->verticalTolerance->value();

    QString type = fileName.split(".").last();

    frames.clear();
    frame = QPixmap();
    QList<Extractor::Box> boxes;

    if (type == "gif") {
        GifExtractor extractor;
        frames = extractor.extractFrames(fileName, alphaThreshold, verticalTolerance);
        frame = extractor.m_atlas;
        boxes = extractor.m_atlas_index;
    }
    else if ((type == "png") || (type == "jpg") ||  (type == "bmp") || (type == "gif")) {
        SpriteExtractor extractor;
        frames = extractor.extractFrames(fileName, alphaThreshold, verticalTolerance);
        frame = extractor.m_atlas;
        boxes = extractor.m_atlas_index;
    } else if (type == "json") {

    } else {
        QMessageBox::warning(this, "Erreur d'ouverture", "Format de fichier non supporté.");
        return;
    }
    if (boundingBoxHighlighter) {
        if (ui->graphicsViewLayers->scene()) {
            ui->graphicsViewLayers->scene()->removeItem(boundingBoxHighlighter);
        }
        delete boundingBoxHighlighter;
        boundingBoxHighlighter = nullptr;
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

void MainWindow::on_alphaThreshold_valueChanged(int threshold)
{
    Q_UNUSED(threshold);
    if (!currentFilePath.isEmpty()) {
        processFile(currentFilePath);
    }
}


void MainWindow::on_verticalTolerance_valueChanged(int verticalTolerance)
{
    Q_UNUSED(verticalTolerance);
    if (!currentFilePath.isEmpty()) {
        processFile(currentFilePath);
    }
}


void MainWindow::on_framesList_clicked(const QModelIndex &index)
{
    if (!ui->graphicsViewLayers->scene() || index.row() >= frameBoxes.size()) {
        return; // Pas de scène ou index invalide
    }

    QGraphicsScene *scene = ui->graphicsViewLayers->scene();
    const Extractor::Box &box = frameBoxes.at(index.row());

    // 1. Supprimer l'ancien highlighter s'il existe
    if (boundingBoxHighlighter) {
        scene->removeItem(boundingBoxHighlighter);
        delete boundingBoxHighlighter;
        boundingBoxHighlighter = nullptr;
    }

    // 2. Créer le nouveau rectangle (QGraphicsRectItem)
    // La Box est {x, y, w, h}
    QRectF rect(box.x, box.y, box.w, box.h);

    // 3. Définir le style du dessin (couleur, épaisseur)
    QPen pen(Qt::red); // Utiliser une couleur vive pour le surlignage
    pen.setWidth(2);   // Épaisseur du trait
    pen.setStyle(Qt::DashLine); // Ligne pointillée ou continue (DashLine est souvent bien visible)

    // 4. Dessiner et ajouter à la scène
    boundingBoxHighlighter = new QGraphicsRectItem(rect);
    boundingBoxHighlighter->setPen(pen);
    boundingBoxHighlighter->setBrush(QBrush(QColor(255, 0, 0, 50))); // Remplissage semi-transparent

    scene->addItem(boundingBoxHighlighter);
}

