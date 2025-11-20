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
  ui->framesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  ui->timingLabel->setText(" -> Timing " + QString::number(1000.0  / (double)ui->fps->value(), 'g', 4) + "ms");
  QObject::connect(ui->framesList, &QListView::clicked,
                    this, &MainWindow::on_framesList_clicked);
  QObject::connect(frameModel, &ArrangementModel::mergeRequested,
                    this, &MainWindow::onMergeFrames);
  listDelegate = new FrameDelegate(this);
  ui->framesList->setItemDelegate(listDelegate);
  ui->framesList->viewport()->installEventFilter(this);
  ui->framesList->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(ui->framesList, &QListView::customContextMenuRequested,
                    this, &MainWindow::on_framesList_customContextMenuRequested);
}

void MainWindow::on_framesList_customContextMenuRequested(const QPoint &pos)
{
  // On vérifie si n'importe quel élément est sous la souris
  // Note : On pourrait aussi vérifier si la sélection est non-vide
  // pour garder le menu contextuel utilisable même si la souris n'est pas sur un item.
  QModelIndex index = ui->framesList->indexAt(pos);

         // Utilisation des éléments sélectionnés plutôt que l'index sous la souris
  QModelIndexList selected = ui->framesList->selectionModel()->selectedIndexes();

  if (!selected.isEmpty()) { // On montre le menu si au moins un élément est sélectionné
      QMenu menu(this);
      // Le texte du menu peut être ajusté pour refléter la sélection multiple
      QString actionText = (selected.size() > 1) ? tr("Supprimer les Frames Sélectionnées") : tr("Supprimer la Frame");
      QAction *deleteAction = menu.addAction(actionText);

      QObject::connect(deleteAction, &QAction::triggered,
                        this, &MainWindow::deleteSelectedFrame);

      menu.addSeparator();

      QAction *invertAction = menu.addAction(tr("Inverser la Sélection"));
      QObject::connect(invertAction, &QAction::triggered,
                        this, &MainWindow::invertSelection);
      menu.exec(ui->framesList->viewport()->mapToGlobal(pos));
    }
}

void MainWindow::invertSelection()
{
  QItemSelectionModel *selectionModel = ui->framesList->selectionModel();
  if (!selectionModel) {
      return;
    }

  QItemSelection newSelection;
  int rowCount = frameModel->rowCount();

  for (int row = 0; row < rowCount; ++row) {
      QModelIndex index = frameModel->index(row, 0);

      if (!selectionModel->isSelected(index)) {
          QItemSelection selection(index, index);
          newSelection.merge(selection, QItemSelectionModel::Select);
        }
    }

  selectionModel->setCurrentIndex(QModelIndex(), QItemSelectionModel::Clear);
  selectionModel->select(newSelection, QItemSelectionModel::ClearAndSelect);
}

void MainWindow::deleteFrame(int row)
{
  if (row < 0 || row >= frameBoxes.size()) {
      return;
    }

         // 1. SUPPRESSION des données internes
  frameBoxes.removeAt(row);
  frames.removeAt(row); // Si vous utilisez cette liste pour la prévisualisation/animation

         // 2. SUPPRESSION du modèle (visuel)
  frameModel->removeRow(row);

         // 3. Nettoyage et rafraîchissement
         // Si la frame supprimée était sélectionnée ou mise en évidence
  if (boundingBoxHighlighter) {
      ui->graphicsViewLayers->scene()->removeItem(boundingBoxHighlighter);
      delete boundingBoxHighlighter;
      boundingBoxHighlighter = nullptr;
    }

         // Si vous aviez un aperçu actif, vous pouvez le mettre à jour ici.

  qDebug() << "Frame supprimée. Frames restantes:" << frameBoxes.size();
}

void MainWindow::deleteSelectedFrame()
{
  QModelIndexList selectedIndexes = ui->framesList->selectionModel()->selectedIndexes();

  if (selectedIndexes.isEmpty()) {
      return;
    }

         // 1. Trier les index par ordre décroissant (important pour la suppression)
  std::sort(selectedIndexes.begin(), selectedIndexes.end(), [](const QModelIndex& a, const QModelIndex& b) {
    return a.row() > b.row(); // Trier par row décroissant
  });

         // 2. Supprimer la sélection actuelle du QListView pour éviter des problèmes d'index
  ui->framesList->selectionModel()->clearSelection();

         // 3. Itérer et supprimer du plus grand index vers le plus petit
  for (const QModelIndex &index : selectedIndexes) {
      int rowToDelete = index.row();
      this->deleteFrame(rowToDelete);
    }

         // 4. Nettoyer l'highlighter après toutes les suppressions
  if (boundingBoxHighlighter) {
      ui->graphicsViewLayers->scene()->removeItem(boundingBoxHighlighter);
      delete boundingBoxHighlighter;
      boundingBoxHighlighter = nullptr;
    }
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
          QPoint pos = dmEvent->pos();
          QModelIndex index = ui->framesList->indexAt(pos);

                 // On désactive TOUJOURS l'indicateur par défaut de Qt car on dessine le nôtre
          ui->framesList->setDropIndicatorShown(false);

          if (index.isValid()) {
              // Récupérer la géométrie visuelle de l'item survolé
              QRect rect = ui->framesList->visualRect(index);

                     // Calculer la position de la souris relative à l'item (0 à width)
              int relativeX = pos.x() - rect.left();
              int width = rect.width();

                     // ZONES DE DÉTECTION
                     // Marge de 20% sur les bords pour l'insertion
              int margin = width * 0.2;

              if (relativeX < margin) {
                  // --- CAS : INSERTION GAUCHE ---
                  listDelegate->setHighlight(index.row(), FrameDelegate::InsertLeft);
                  clearMergeHighlight(); // Pas de visuel sur l'atlas
                }
              else if (relativeX > (width - margin)) {
                  // --- CAS : INSERTION DROITE ---
                  listDelegate->setHighlight(index.row(), FrameDelegate::InsertRight);
                  clearMergeHighlight(); // Pas de visuel sur l'atlas
                }
              else {
                  // --- CAS : FUSION (CENTRE) ---
                  listDelegate->setHighlight(index.row(), FrameDelegate::Merge);
                  setMergeHighlight(index, true); // On active le visuel sur l'atlas
                }
            } else {
              // Si on est dans le vide (après la dernière frame)
              listDelegate->setHighlight(-1, FrameDelegate::None);
              clearMergeHighlight();
            }

          ui->framesList->viewport()->update();
          // Important : ne pas bloquer l'événement pour que le drag continue
        }
      else if (event->type() == QEvent::DragLeave || event->type() == QEvent::Drop) {
          // Nettoyage
          listDelegate->setHighlight(-1, FrameDelegate::None);
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

