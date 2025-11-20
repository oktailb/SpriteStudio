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
      , animationTimer(new QTimer(this))
      , currentAnimationFrameIndex(0)
{
  ui->setupUi(this);
  setAcceptDrops(true);
  timerId = startTimer(100);
  ready = true;
  ui->framesList->setModel(frameModel);
  ui->framesList->setViewMode(QListView::IconMode);
  ui->framesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  QObject::connect(ui->framesList->selectionModel(), &QItemSelectionModel::selectionChanged,
                    this, &MainWindow::startAnimation);
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
  QObject::connect(animationTimer, &QTimer::timeout,
                    this, &MainWindow::updateAnimation);
  QObject::connect(ui->fps, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, &MainWindow::startAnimation);
  ui->graphicsViewResult->setScene(new QGraphicsScene(this));
  startAnimation();
}

void MainWindow::startAnimation()
{
  if (animationTimer->isActive()) {
      animationTimer->stop();
    }

  selectedFrameRows.clear();
  QModelIndexList selected = ui->framesList->selectionModel()->selectedIndexes();

  for (const QModelIndex &index : selected) {
      selectedFrameRows.append(index.row());
    }

  if (selectedFrameRows.isEmpty()) {
      stopAnimation();
      return;
    }

  int fpsValue = ui->fps->value();
  int intervalMs = (fpsValue > 0) ? (1000 / fpsValue) : 100;

  currentAnimationFrameIndex = 0;

  animationTimer->start(intervalMs);
}

void MainWindow::stopAnimation()
{
  animationTimer->stop();
  currentAnimationFrameIndex = 0;
}

void MainWindow::updateAnimation()
{
  if (selectedFrameRows.isEmpty() || frames.isEmpty()) {
      stopAnimation();
      return;
    }

  int frameListIndex = selectedFrameRows.at(currentAnimationFrameIndex);

  if (frameListIndex < 0 || frameListIndex >= frames.size()) {
      qWarning() << "Erreur: Index de frame invalide pour l'animation.";
      stopAnimation();
      return;
    }

  const QPixmap &currentFrame = frames.at(frameListIndex);

  QGraphicsScene *scene = ui->graphicsViewResult->scene();
  scene->clear();

  scene->setSceneRect(0, 0, maxFrameWidth, maxFrameHeight);

  QGraphicsPixmapItem *item =scene->addPixmap(currentFrame);

  // Calculer le décalage pour centrer l'image dans la zone de la scène
  qreal x_offset = (maxFrameWidth - currentFrame.width()) / 2.0;
  qreal y_offset = (maxFrameHeight - currentFrame.height()) / 2.0;
  item->setPos(x_offset, y_offset); // Centrer la frame plus petite

  currentAnimationFrameIndex++;
  if (currentAnimationFrameIndex >= selectedFrameRows.size()) {
      currentAnimationFrameIndex = 0; // Boucler
    }
}

void MainWindow::on_framesList_customContextMenuRequested(const QPoint &pos)
{
  // On vérifie si n'importe quel élément est sous la souris
  // Note : On pourrait aussi vérifier si la sélection est non-vide
  // pour garder le menu contextuel utilisable même si la souris n'est pas sur un item.
  QModelIndex index = ui->framesList->indexAt(pos);

         // Utilisation des éléments sélectionnés plutôt que l'index sous la souris
  QModelIndexList selected = ui->framesList->selectionModel()->selectedIndexes();

  if (!selected.isEmpty()) {
      QMenu menu(this);
      QString actionText = (selected.size() > 1) ? tr("Supprimer les Frames Sélectionnées") : tr("Supprimer la Frame");
      QAction *deleteAction = menu.addAction(actionText);

      QObject::connect(deleteAction, &QAction::triggered,
                        this, &MainWindow::deleteSelectedFrame);

      menu.addSeparator();

      QAction *invertAction = menu.addAction(tr("Inverser la Sélection"));
      QObject::connect(invertAction, &QAction::triggered,
                        this, &MainWindow::invertSelection);

      QAction *reverseOrderAction = menu.addAction(tr("Renverser l'Ordre des Frames Sélectionnées"));
      QObject::connect(reverseOrderAction, &QAction::triggered,
                        this, &MainWindow::reverseSelectedFramesOrder);

      menu.exec(ui->framesList->viewport()->mapToGlobal(pos));
    }
}

void MainWindow::reverseSelectedFramesOrder()
{
  QModelIndexList selectedIndexes = ui->framesList->selectionModel()->selectedIndexes();

  if (selectedIndexes.isEmpty()) {
      return;
    }

  std::sort(selectedIndexes.begin(), selectedIndexes.end(), [](const QModelIndex& a, const QModelIndex& b) {
    return a.row() < b.row();
  });

  int firstRow = selectedIndexes.first().row();
  int lastRow = selectedIndexes.last().row();

  if (firstRow == lastRow) {
      return;
    }

  int i = firstRow;
  int j = lastRow;

  frameModel->blockSignals(true);

  while (i < j) {
      Extractor::Box tempBox = frameBoxes[i];
      frameBoxes[i] = frameBoxes[j];
      frameBoxes[j] = tempBox;

      QPixmap tempFrame = frames[i];
      frames[i] = frames[j];
      frames[j] = tempFrame;

      QStandardItem *itemA = frameModel->item(i, 0)->clone();
      QStandardItem *itemB = frameModel->item(j, 0)->clone();

      delete frameModel->takeItem(i, 0);
      delete frameModel->takeItem(j, 0);

      frameModel->setItem(i, 0, itemB);
      frameModel->setItem(j, 0, itemA);

      frameModel->setItem(i, 0, itemB->clone());
      frameModel->setItem(j, 0, itemA->clone());

      i++;
      j--;
    }

  frameModel->blockSignals(false);

  ui->framesList->update();
  ui->framesList->selectionModel()->select(selectedIndexes.first(), QItemSelectionModel::ClearAndSelect); // Resélectionner le premier item du nouveau bloc

  startAnimation();
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
  maxFrameWidth = 0;
  maxFrameHeight = 0;
  for (const QPixmap &pixmap : frameList) {
      if (pixmap.width() > maxFrameWidth) {
          maxFrameWidth = pixmap.width();
        }
      if (pixmap.height() > maxFrameHeight) {
          maxFrameHeight = pixmap.height();
        }
    }

  if (frameList.isEmpty()) {
      maxFrameWidth = 0;
      maxFrameHeight = 0;
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


void MainWindow::on_actionExport_triggered()
{
  // Vérifier si des frames existent
  if (frames.isEmpty()) {
      QMessageBox::warning(this, tr("Exportation impossible"), tr("Veuillez charger ou créer des frames avant d'exporter."));
      return;
    }

         // 1. Définir les formats d'exportation disponibles
  const QString filter = tr("Sprite Atlas (*.png *.json);;PNG Image (*.png)");

  // 2. Ouvrir la boîte de dialogue d'enregistrement
  // Le nom de fichier sélectionné servira de BASE pour le nom de projet et d'atlas.
  QString selectedFile = QFileDialog::getSaveFileName(
      this,
      tr("Exporter le Sprite Atlas"),
      QDir::homePath(), // Chemin par défaut
      filter
      );

  if (selectedFile.isEmpty()) {
      return; // Annulé par l'utilisateur
    }

         // 3. Déterminer le format et le nom de base
  QFileInfo fileInfo(selectedFile);
  QString filePath = fileInfo.absolutePath(); // Le dossier de destination
  QString baseName = fileInfo.completeBaseName(); // Le nom du fichier sans extension
  QString extension = fileInfo.suffix().toLower(); // L'extension sélectionnée

  QString format;
  if (extension == "json") {
      format = "JSON_ATLAS"; // Notre format "Texture Packer"
    } else if (extension == "png") {
      // Si l'utilisateur a sélectionné *.png, nous allons demander confirmation pour exporter
      // seulement l'image ou l'image + métadonnées. Pour l'instant, simplifions:
      format = "JSON_ATLAS";
    } else {
      // Cas par défaut/inattendu. Utiliser notre format principal.
      format = "JSON_ATLAS";
    }

         // 4. Lancer l'exportation
  exportSpriteSheet(filePath, baseName, format);
}

void MainWindow::exportSpriteSheet(const QString &basePath, const QString &projectName, const QString &format)
{
  // Vérification de la méthode d'export
  if (format != "JSON_ATLAS") {
      // Pour les futurs formats (ex: XML, Godot, Unity)
      QMessageBox::warning(this, tr("Exportation"), tr("Format d'exportation non supporté pour le moment."));
      return;
    }

  // --- 1. Générer l'Image Atlas (Sprite Sheet) ---
  // Puisque les frames dans 'frames' sont déjà bien découpées, nous devons les remonter
  // dans une seule grande image, en utilisant les dimensions maximales que nous avons déjà.

  int totalFrames = frames.size();
  if (totalFrames == 0) return;

         // Détermination de la grille (similaire à GifExtractor, mais plus simple)
         // Nous utiliserons le même arrangement en grille que dans GifExtractor
  int w = maxFrameWidth;
  int h = maxFrameHeight;
  int nb_cols = (int)std::floor(std::sqrt(totalFrames));
  if (nb_cols == 0) nb_cols = 1;
  int nb_lines = (int)std::ceil((double)totalFrames / nb_cols);

  QImage atlasImage(w * nb_cols, h * nb_lines, QImage::Format_ARGB32_Premultiplied);
  atlasImage.fill(Qt::transparent);
  QPainter painter(&atlasImage);

  if (!painter.isActive()) {
      QMessageBox::critical(this, tr("Erreur Critique"), tr("Impossible de démarrer le peintre pour l'atlas."));
      return;
    }

  QJsonArray framesArray; // JSON array pour les métadonnées

  for (int i = 0; i < totalFrames; ++i) {
      const QPixmap &currentPixmap = frames.at(i);
      QImage currentImage = currentPixmap.toImage();

      int line = i / nb_cols;
      int col = i % nb_cols;
      int x = col * w;
      int y = line * h;

             // Centrer la frame dans sa case (basé sur la logique d'animation)
      int x_offset = x + (w - currentImage.width()) / 2;
      int y_offset = y + (h - currentImage.height()) / 2;

      painter.drawImage(QPoint(x_offset, y_offset), currentImage);

             // --- 2. Construire les Métadonnées JSON (Coordonnées sur l'Atlas) ---
             // Chaque frame a maintenant une position fixe sur l'atlas

      QJsonObject frameData;
      frameData["filename"] = QString("frame_%1").arg(i, 4, 10, QChar('0'));

      QJsonObject frameRect;
      frameRect["x"] = x_offset;
      frameRect["y"] = y_offset;
      frameRect["w"] = currentImage.width();
      frameRect["h"] = currentImage.height();

      frameData["frame"] = frameRect;

      // Simuler des métadonnées de texture packer (ajoutez plus si nécessaire)
      frameData["rotated"] = false;
      frameData["trimmed"] = false;
      frameData["spriteSourceSize"] = frameRect; // Simplifié pour l'exemple

      framesArray.append(frameData);
    }

  painter.end();

  // --- 3. Sauvegarde des Fichiers ---

  // Nom des fichiers de sortie
  QString pngFilePath = QDir(basePath).filePath(projectName + ".png");
  QString jsonFilePath = QDir(basePath).filePath(projectName + ".json");

  // a) Sauvegarde PNG
  if (!atlasImage.save(pngFilePath, "PNG")) {
      QMessageBox::critical(this, tr("Erreur d'écriture"), tr("Impossible d'écrire le fichier PNG. Vérifiez les permissions."));
      return;
    }

         // b) Sauvegarde JSON
  QJsonObject root;
  root["frames"] = framesArray;

  QJsonObject meta;
  meta["image"] = projectName + ".png";
  meta["size"] = QJsonObject({
      {"w", atlasImage.width()},
      {"h", atlasImage.height()}
  });
  meta["format"] = "RGBA8888";
  root["meta"] = meta;

  QJsonDocument doc(root);

  QFile jsonFile(jsonFilePath);
  if (jsonFile.open(QIODevice::WriteOnly)) {
      jsonFile.write(doc.toJson(QJsonDocument::Indented)); // Format indenté et lisible
      jsonFile.close();

      QMessageBox::information(this, tr("Exportation réussie"), tr("L'atlas et les métadonnées ont été exportés avec succès : %1").arg(basePath));
    } else {
      QMessageBox::critical(this, tr("Erreur d'écriture"), tr("Impossible d'écrire le fichier JSON. Vérifiez les permissions."));
    }
}
