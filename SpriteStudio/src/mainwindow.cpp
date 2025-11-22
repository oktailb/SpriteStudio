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
#include <QMovie>
#include <QGraphicsRectItem>
#include "gifextractor.h"
#include "spriteextractor.h"
#include "jsonextractor.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
      , ui(new Ui::MainWindow)
      , frameModel(new ArrangementModel(this))
      , timerId(0)
      , ready(false)
      , extractor(nullptr)
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
  ui->Play->setVisible(false);
  ui->Pause->setVisible(true);
  ui->graphicsViewLayers->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->graphicsViewLayers, &QWidget::customContextMenuRequested,
           this, &MainWindow::onAtlasContextMenuRequested);

  startAnimation();
}

void MainWindow::onAtlasContextMenuRequested(const QPoint &pos)
{
  // On vérifie qu'on a bien une image chargée
  if (!extractor || extractor->m_atlas.isNull()) return;

  QMenu menu(this);
  QAction *removeBgAction = menu.addAction(tr("Supprimer le fond (Auto)"));

         // On connecte l'action à notre fonction de traitement
  connect(removeBgAction, &QAction::triggered, this, &MainWindow::removeAtlasBackground);

  menu.exec(ui->graphicsViewLayers->mapToGlobal(pos));
}

void MainWindow::removeAtlasBackground()
{
  if (!extractor || extractor->m_atlas.isNull()) return;

  QImage image = extractor->m_atlas.toImage();
  image = image.convertToFormat(QImage::Format_ARGB32);

         // 1. Trouver la couleur de fond (La plus fréquente / Mode)
         // On pourrait aussi prendre le pixel (0,0) par défaut
  QMap<QRgb, int> histogram;
  int maxCount = 0;
  QRgb backgroundColor = 0;

         // Optimisation : on ne scanne qu'un pixel sur 4 pour aller vite
  for (int y = 0; y < image.height(); y += 2) {
      for (int x = 0; x < image.width(); x += 2) {
          QRgb pixel = image.pixel(x, y);
          // On ignore les pixels déjà transparents
          if (qAlpha(pixel) < 10) continue;

          histogram[pixel]++;
          if (histogram[pixel] > maxCount) {
              maxCount = histogram[pixel];
              backgroundColor = pixel;
            }
        }
    }

         // Si on a rien trouvé (image vide ou full transparent)
  if (maxCount == 0) return;

  QColor bgCol(backgroundColor);
  qDebug() << "Fond détecté :" << bgCol.name();

         // 2. Remplacement par la transparence
         // On utilise une petite tolérance pour les artefacts de compression JPG
  int tolerance = 10;

  for (int y = 0; y < image.height(); ++y) {
      for (int x = 0; x < image.width(); ++x) {
          QColor pixCol(image.pixel(x, y));

                 // Calcul de distance simple (Manhattan)
          int diff = std::abs(pixCol.red() - bgCol.red()) +
                     std::abs(pixCol.green() - bgCol.green()) +
                     std::abs(pixCol.blue() - bgCol.blue());

          if (diff <= tolerance) {
              image.setPixel(x, y, Qt::transparent);
            }
        }
    }

         // 3. Mise à jour et Relance de l'extraction
         // Attention : Il faut caster l'extractor vers SpriteExtractor pour accéder à la nouvelle méthode
  SpriteExtractor *spriteExt = dynamic_cast<SpriteExtractor*>(extractor);
  if (spriteExt) {

      // 1. Mettre à jour l'atlas interne de l'extracteur AVANT l'extraction
      extractor->m_atlas = QPixmap::fromImage(image);

             // 2. Relancer l'extraction (cela va recalculer maxFrameWidth/Height)
      spriteExt->extractFromPixmap(ui->alphaThreshold->value(),
                                   ui->verticalTolerance->value());

             // 3. Mettre à jour la vue principale (Layers) avec la nouvelle image transparente
      QGraphicsScene *sceneLayers = ui->graphicsViewLayers->scene();
      if (!sceneLayers) {
          sceneLayers = new QGraphicsScene(this);
          ui->graphicsViewLayers->setScene(sceneLayers);
        }
      sceneLayers->clear();

             // On ajoute la nouvelle image modifiée
      QGraphicsPixmapItem *item = sceneLayers->addPixmap(extractor->m_atlas);

             // On ajuste la taille de la scène à la taille de l'atlas
      sceneLayers->setSceneRect(extractor->m_atlas.rect());

             // On demande à la vue de tout afficher proprement
      ui->graphicsViewLayers->fitInView(item, Qt::KeepAspectRatio);

             // 4. Mettre à jour la liste des frames
      populateFrameList(extractor->m_frames, extractor->m_atlas_index);

             // 5. Réinitialiser l'animation pour prendre en compte les nouvelles dimensions
      stopAnimation();
      startAnimation(); // Relance avec les nouvelles tailles (maxFrameWidth corrigé)
    }
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

  int frameCount = selectedFrameRows.size();
  int fps = ui->fps->value();
  if (fps <= 0) fps = 1;

  double msPerFrame = 1000.0 / (double)fps;
  int totalDurationMs = (int)(frameCount * msPerFrame);

  QTime durationTime(0, 0, 0);
  durationTime = durationTime.addMSecs(totalDurationMs);

  ui->timeTo->setDisplayFormat("mm:ss:zzz");
  ui->timeTo->setTime(durationTime);

  if (frameCount > 0) {
      ui->sliderFrom->setMaximum(frameCount - 1);
    } else {
      ui->sliderFrom->setMaximum(0);
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
  if (selectedFrameRows.isEmpty() || extractor->m_frames.isEmpty()) {
      stopAnimation();
      return;
    }

  int frameListIndex = selectedFrameRows.at(currentAnimationFrameIndex);

  if (frameListIndex < 0 || frameListIndex >= extractor->m_frames.size()) {
      qWarning() << "Erreur: Index de frame invalide pour l'animation.";
      stopAnimation();
      return;
    }

  ui->sliderFrom->blockSignals(true);
  ui->sliderFrom->setValue(currentAnimationFrameIndex);
  ui->sliderFrom->blockSignals(false);

  int fps = ui->fps->value();
  if (fps <= 0) fps = 60;

  double msPerFrame = 1000.0 / (double)fps;
  int currentMs = (int)(currentAnimationFrameIndex * msPerFrame);

  QTime currentTime(0, 0, 0);
  currentTime = currentTime.addMSecs(currentMs);

  ui->timeFrom->setDisplayFormat("mm:ss:zzz");
  ui->timeFrom->setTime(currentTime);


  const QPixmap &currentFrame = extractor->m_frames.at(frameListIndex);

  QGraphicsScene *scene = ui->graphicsViewResult->scene();
  scene->clear();

  scene->setSceneRect(0, 0, extractor->m_maxFrameWidth, extractor->m_maxFrameHeight);

  QGraphicsPixmapItem *item =scene->addPixmap(currentFrame);

         // Calculer le décalage pour centrer l'image dans la zone de la scène
  qreal x_offset = (extractor->m_maxFrameWidth - currentFrame.width()) / 2.0;
  qreal y_offset = (extractor->m_maxFrameHeight - currentFrame.height()) / 2.0;
  item->setPos(x_offset, y_offset); // Centrer la frame plus petite

  currentAnimationFrameIndex++;
  if (currentAnimationFrameIndex >= selectedFrameRows.size()) {
      currentAnimationFrameIndex = 0; // Boucler
    }
}

void MainWindow::on_framesList_customContextMenuRequested(const QPoint &pos)
{
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
      Extractor::Box tempBox = extractor->m_atlas_index[i];
      extractor->m_atlas_index[i] = extractor->m_atlas_index[j];
      extractor->m_atlas_index[j] = tempBox;

      QPixmap tempFrame = extractor->m_frames[i];
      extractor->m_frames[i] = extractor->m_frames[j];
      extractor->m_frames[j] = tempFrame;

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
  if (row < 0 || row >= extractor->m_atlas_index.size()) {
      return;
    }

         // 1. SUPPRESSION des données internes
  extractor->m_atlas_index.removeAt(row);
  extractor->m_frames.removeAt(row); // Si vous utilisez cette liste pour la prévisualisation/animation

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

  qDebug() << "Frame supprimée. Frames restantes:" << extractor->m_atlas_index.size();
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
  if (row >= 0 && row < extractor->m_atlas_index.size()) {
      // On récupère la box correspondante
      Extractor::Box box = extractor->m_atlas_index[row];
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
          QPoint pos = dmEvent->position().toPoint();
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
  if (sourceRow < 0 || sourceRow >= extractor->m_atlas_index.size() ||
      targetRow < 0 || targetRow >= extractor->m_atlas_index.size()) {
      return;
    }

         // 1. Calcul et Fusion (Identique à avant)
  Extractor::Box srcBox = extractor->m_atlas_index[sourceRow];
  Extractor::Box tgtBox = extractor->m_atlas_index[targetRow];

  QRect srcRect(srcBox.x, srcBox.y, srcBox.w, srcBox.h);
  QRect tgtRect(tgtBox.x, tgtBox.y, tgtBox.w, tgtBox.h);
  QRect unitedRect = srcRect.united(tgtRect);

  QPixmap mergedPixmap = this->extractor->m_atlas.copy(unitedRect);

         // 2. Mise à jour des données internes de la CIBLE
  Extractor::Box newBox;
  newBox.x = unitedRect.x();
  newBox.y = unitedRect.y();
  newBox.w = unitedRect.width();
  newBox.h = unitedRect.height();

  extractor->m_atlas_index[targetRow] = newBox;
  extractor->m_frames[targetRow] = mergedPixmap;

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

  extractor->m_atlas_index.removeAt(sourceRow);
  extractor->m_frames.removeAt(sourceRow);

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
  //extractor->m_atlas_index.clear();

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
      extractor->m_atlas_index.append(box);
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
  QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.json)"));
  currentFilePath = fileName;
  processFile(currentFilePath);
  ui->verticalTolerance->setValue(extractor->m_maxFrameHeight / 3);
}

void MainWindow::processFile(const QString &fileName)
{
  int alphaThreshold = ui->alphaThreshold->value();
  int verticalTolerance = ui->verticalTolerance->value();

  currentFilePath = fileName;
  QFileInfo fileInfo(fileName);
  QString extension = fileInfo.suffix().toLower();

         // Nettoyage de l'extracteur précédent
  if (extractor) {
      delete extractor;
      extractor = nullptr;
    }

  if (extension == "gif") {
      QMovie movie(fileName);
      if (!movie.isValid()) {
          QMessageBox::critical(this, tr("Erreur de Fichier"), tr("Le fichier GIF n'est pas valide."));
          return;
        }

      // Vérification du nombre de frames
      int frameCount = movie.frameCount();

      if (frameCount > 1) {
          // CAS 1 : GIF animé (plusieurs frames) -> Utiliser GifExtractor
          qDebug() << "Fichier GIF : Détecté" << frameCount << "frames. Utilisation de GifExtractor.";
          extractor = new GifExtractor(this);
        } else {
          // CAS 2 : GIF statique (0 ou 1 frame) -> Utiliser SpriteExtractor
          // Cela permet l'extraction de sprites et l'application du "Magic Wand" (suppression de fond).
          qDebug() << "Fichier GIF : Détecté 1 frame ou moins. Utilisation de SpriteExtractor.";
          extractor = new SpriteExtractor(this);
        }
    }
  else if ((extension == "png") || (extension == "jpg") || (extension == "jpeg") ||  (extension == "bmp") || (extension == "gif")) {
      extractor = new SpriteExtractor();
    } else if (extension == "json") {
      extractor = new JsonExtractor();
    } else {
      QMessageBox::warning(this, "Erreur d'ouverture", "Format de fichier non supporté.");
      return;
    }
  connect(extractor, &Extractor::extractionFinished,
           this, [this]() {

             //this->setupGraphicsView(extractor->m_atlas);
             this->populateFrameList(extractor->m_frames, extractor->m_atlas_index);
             // On s'assure que l'animation est lancée ou arrêtée correctement après l'extraction
             this->stopAnimation();
             this->startAnimation();
             ui->verticalTolerance->setValue(extractor->m_maxFrameHeight / 3);
           });
  extractor->extractFrames(fileName, alphaThreshold, verticalTolerance);
  if (boundingBoxHighlighter) {
      if (ui->graphicsViewLayers->scene()) {
          ui->graphicsViewLayers->scene()->removeItem(boundingBoxHighlighter);
        }
      delete boundingBoxHighlighter;
      boundingBoxHighlighter = nullptr;
    }
  if (!extractor->m_frames.isEmpty()) {
      auto view = ui->graphicsViewLayers;
      auto scene = new QGraphicsScene(view);

      QGraphicsPixmapItem *item = new QGraphicsPixmapItem(extractor->m_atlas);
      scene->addItem(item);
      item->setPos(0, 0);
      view->setScene(scene);
      view->show();

      populateFrameList(extractor->m_frames, extractor->m_atlas_index);
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
  if (!extractor) {
      if (!currentFilePath.isEmpty()) {
          processFile(currentFilePath);
        }
    }
  else
    {
      extractor->extractFromPixmap(ui->alphaThreshold->value(), ui->verticalTolerance->value());
    }
}


void MainWindow::on_framesList_clicked(const QModelIndex &index)
{
  if (!ui->graphicsViewLayers->scene() || index.row() >= extractor->m_atlas_index.size()) {
      return; // Pas de scène ou index invalide
    }

  QGraphicsScene *scene = ui->graphicsViewLayers->scene();
  const Extractor::Box &box = extractor->m_atlas_index.at(index.row());

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
  if (extractor->m_frames.isEmpty()) {
      QMessageBox::warning(this, tr("Exportation impossible"), tr("Veuillez charger ou créer des frames avant d'exporter."));
      return;
    }
  Extractor * extractorOut = nullptr;
  const QString filter = tr("Sprite Atlas (*.png *.json);;PNG Image (*.png)");

  QString selectedFile = QFileDialog::getSaveFileName(
      this,
      tr("Exporter le Sprite Atlas"),
      QDir::homePath(), // Chemin par défaut
      filter
      );

  if (selectedFile.isEmpty()) {
      return; // Annulé par l'utilisateur
    }

  QFileInfo fileInfo(selectedFile);
  QString filePath = fileInfo.absolutePath();
  QString baseName = fileInfo.completeBaseName();
  QString extension = fileInfo.suffix().toLower();

  if (extension == "json") {
      extractorOut = new JsonExtractor();
    } else if (extension == "png") {
      extractorOut = new SpriteExtractor();
    } else if (extension == "gif") {
      extractorOut = new GifExtractor();
    } else {
      extractorOut = new JsonExtractor();
    }

  extractorOut->exportFrames(filePath, baseName, extractor);
}

void MainWindow::on_Play_clicked()
{
  if (selectedFrameRows.isEmpty()) {
      startAnimation();
      return;
    }

  int fpsValue = ui->fps->value();
  int intervalMs = (fpsValue > 0) ? (1000 / fpsValue) : 100;
  animationTimer->setInterval(intervalMs);

  if (!animationTimer->isActive()) {
      animationTimer->start();
    }

  ui->Pause->setVisible(true);
  ui->Play->setVisible(false);
}


void MainWindow::on_Pause_clicked()
{
  if (animationTimer->isActive()) {
      animationTimer->stop();
    }
  ui->Play->setVisible(true);
  ui->Pause->setVisible(false);

}

