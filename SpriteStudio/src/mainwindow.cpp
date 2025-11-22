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
  // Enable drag and drop events for the main window (to handle file drops).
  setAcceptDrops(true);
  // Start a low-frequency system timer (100ms interval) for general background checks/updates.
  timerId = startTimer(100);

  // --- Frame List (QListView) Setup ---

  // Assign the custom model to the frames list view.
  ui->framesList->setModel(frameModel);
  ui->framesList->setViewMode(QListView::IconMode);
  ui->framesList->setSelectionMode(QAbstractItemView::ExtendedSelection);

  // Connect the selection change signal to automatically start/restart the animation
  // when the user selects or deselects frames.
  QObject::connect(ui->framesList->selectionModel(), &QItemSelectionModel::selectionChanged,
                    this, &MainWindow::startAnimation);
  ui->timingLabel->setText(" -> Timing " + QString::number(1000.0  / (double)ui->fps->value(), 'g', 4) + "ms");

  // Connect the click signal to the slot that handles highlighting the frame in the atlas view.
  QObject::connect(ui->framesList, &QListView::clicked,
                    this, &MainWindow::on_framesList_clicked);
  QObject::connect(frameModel, &ArrangementModel::mergeRequested,
                    this, &MainWindow::onMergeFrames);
  listDelegate = new FrameDelegate(this);
  ui->framesList->setItemDelegate(listDelegate);

  // Install the main window as an event filter on the list view's viewport.
  // This allows the main window to intercept mouse/drag events for the custom drag-and-drop delegate logic.
  ui->framesList->viewport()->installEventFilter(this);
  ui->framesList->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(ui->framesList, &QListView::customContextMenuRequested,
                    this, &MainWindow::on_framesList_customContextMenuRequested);

  // --- Animation Setup ---
  QObject::connect(animationTimer, &QTimer::timeout,
                    this, &MainWindow::updateAnimation);
  QObject::connect(ui->fps, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, &MainWindow::startAnimation);

  // --- Graphics View Setup ---
  ui->graphicsViewResult->setScene(new QGraphicsScene(this));
  ui->Play->setVisible(false);
  ui->Pause->setVisible(true);
  ui->graphicsViewLayers->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->graphicsViewLayers, &QWidget::customContextMenuRequested,
           this, &MainWindow::onAtlasContextMenuRequested);

  // Configure the view that displays the sprite atlas (ui->graphicsViewLayers).
  // QGraphicsView::FitInView ensures the entire scene fits inside the view, scaling if necessary.
  ui->graphicsViewLayers->setRenderHint(QPainter::Antialiasing, true);
  ui->graphicsViewLayers->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
  ui->graphicsViewLayers->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  ui->graphicsViewLayers->setResizeAnchor(QGraphicsView::AnchorViewCenter);
  ui->graphicsViewLayers->fitInView(ui->graphicsViewLayers->sceneRect(), Qt::KeepAspectRatio);

  // Configure the view that displays the animation preview (ui->graphicsViewResult).
  ui->graphicsViewResult->setRenderHint(QPainter::Antialiasing, true);
  ui->graphicsViewResult->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
  ui->graphicsViewResult->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  ui->graphicsViewResult->setResizeAnchor(QGraphicsView::AnchorViewCenter);
  ui->graphicsViewResult->fitInView(ui->graphicsViewResult->sceneRect(), Qt::KeepAspectRatio);

  // Start the animation immediately (it will likely run with a single frame until a file is loaded).
  startAnimation();
  // Set the ready flag to true now that basic initialization is complete.
  ready = true;
}

void MainWindow::onAtlasContextMenuRequested(const QPoint &pos)
{
  // Cjeck if a frame is loaded
  if (!extractor || extractor->m_atlas.isNull())
    return;

  QMenu menu(this);
  QAction *removeBgAction = menu.addAction(tr("Supprimer le fond (Auto)"));

  // Connect the action to the treatment method
  connect(removeBgAction, &QAction::triggered, this, &MainWindow::removeAtlasBackground);

  menu.exec(ui->graphicsViewLayers->mapToGlobal(pos));
}

void MainWindow::removeAtlasBackground()
{
  if (!extractor || extractor->m_atlas.isNull()) return;

  QImage image = extractor->m_atlas.toImage();
  image = image.convertToFormat(QImage::Format_ARGB32);

  // Find the most frequet color
  QMap<QRgb, int> histogram;
  int maxCount = 0;
  QRgb backgroundColor = 0;

  // Optimisation : scan only even pixels / lines to be faster. Statiscally same result
  for (int y = 0; y < image.height(); y += 2) {
      for (int x = 0; x < image.width(); x += 2) {
          QRgb pixel = image.pixel(x, y);
          // Ignore pixels near of pure alpha already
          if (qAlpha(pixel) < 10) continue;

          histogram[pixel]++;
          if (histogram[pixel] > maxCount) {
              maxCount = histogram[pixel];
              backgroundColor = pixel;
            }
        }
    }

  // Case of pure transparent background
  if (maxCount == 0)
    return;

  QColor bgCol(backgroundColor);

  // 2. Replacement of the detected color by alpha
  // Use a tolerance value to avoid compression artefacts
  int tolerance = 10;

  for (int y = 0; y < image.height(); ++y) {
      for (int x = 0; x < image.width(); ++x) {
          QColor pixCol(image.pixel(x, y));

          // Compute distance from tolerance
          int diff = std::abs(pixCol.red() - bgCol.red()) +
                     std::abs(pixCol.green() - bgCol.green()) +
                     std::abs(pixCol.blue() - bgCol.blue());

          if (diff <= tolerance) {
              image.setPixel(x, y, Qt::transparent);
            }
        }
    }

  // Re-launch sprites extraction with a cleared background
  SpriteExtractor *spriteExt = dynamic_cast<SpriteExtractor*>(extractor);
  if (spriteExt) {

      // 1. Update sprite atlas on extractor data model
      extractor->m_atlas = QPixmap::fromImage(image);

      // 2. Recompute extraction and max width/height
      spriteExt->extractFromPixmap(ui->alphaThreshold->value(),
                                   ui->verticalTolerance->value());

      // 3. Refresh main view
      QGraphicsScene *sceneLayers = ui->graphicsViewLayers->scene();
      if (!sceneLayers) {
          sceneLayers = new QGraphicsScene(this);
          ui->graphicsViewLayers->setScene(sceneLayers);
        }
      sceneLayers->clear();

      // Add modified picture
      QGraphicsPixmapItem *item = sceneLayers->addPixmap(extractor->m_atlas);

      // Re-adjust scene to the picture size
      sceneLayers->setSceneRect(extractor->m_atlas.rect());
      ui->graphicsViewLayers->fitInView(item, Qt::KeepAspectRatio);

      // 4. Refresn frame list view
      populateFrameList(extractor->m_frames, extractor->m_atlas_index);

      // 5. Relaunch animation in consequence
      stopAnimation();
      startAnimation();
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

  // Center the animation
  qreal x_offset = (extractor->m_maxFrameWidth - currentFrame.width()) / 2.0;
  qreal y_offset = (extractor->m_maxFrameHeight - currentFrame.height()) / 2.0;
  item->setPos(x_offset, y_offset);

  currentAnimationFrameIndex++;
  if (currentAnimationFrameIndex >= selectedFrameRows.size()) {
      currentAnimationFrameIndex = 0; // Forever loop
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
  ui->framesList->selectionModel()->select(selectedIndexes.first(), QItemSelectionModel::ClearAndSelect); // Reselect first item of the block

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

  // 1. Cleanup internal data
  extractor->m_atlas_index.removeAt(row);
  extractor->m_frames.removeAt(row);

  // 2. Cleanup model
  frameModel->removeRow(row);

  // 3. Cleanup and refresh if the frame was selected
  if (boundingBoxHighlighter) {
      ui->graphicsViewLayers->scene()->removeItem(boundingBoxHighlighter);
      delete boundingBoxHighlighter;
      boundingBoxHighlighter = nullptr;
    }
}

void MainWindow::deleteSelectedFrame()
{
  QModelIndexList selectedIndexes = ui->framesList->selectionModel()->selectedIndexes();

  if (selectedIndexes.isEmpty()) {
      return;
    }

  // 1. Sort by decreasing index (allows more safe deletion on the list)
  std::sort(selectedIndexes.begin(), selectedIndexes.end(), [](const QModelIndex& a, const QModelIndex& b) {
    return a.row() > b.row();
  });

  // 2. Remove selection to avoid visualisation of deleted data
  ui->framesList->selectionModel()->clearSelection();

  // 3. Delete from upper index to lower
  for (const QModelIndex &index : selectedIndexes) {
      int rowToDelete = index.row();
      this->deleteFrame(rowToDelete);
    }

  // 4. Cleanup highlighter
  if (boundingBoxHighlighter) {
      ui->graphicsViewLayers->scene()->removeItem(boundingBoxHighlighter);
      delete boundingBoxHighlighter;
      boundingBoxHighlighter = nullptr;
    }
}

void MainWindow::setMergeHighlight(const QModelIndex &index, bool show)
{
  // PReventative cleanup
  if (!show || !index.isValid()) {
      clearMergeHighlight();
      return;
    }

  int row = index.row();
  if (row >= 0 && row < extractor->m_atlas_index.size()) {
      // Get the matching box
      Extractor::Box box = extractor->m_atlas_index[row];
      QRectF rect(box.x, box.y, box.w, box.h);

      // Rectangle lazy creation
      if (!mergeHighlighter) {
          mergeHighlighter = new QGraphicsRectItem();
          QPen pen(Qt::magenta);
          pen.setWidth(3);
          pen.setStyle(Qt::DotLine);
          mergeHighlighter->setPen(pen);
          // Important : Ensure to be over the image
          mergeHighlighter->setZValue(10);

          if (ui->graphicsViewLayers->scene()) {
              ui->graphicsViewLayers->scene()->addItem(mergeHighlighter);
            }
        }

      // Force reload if the scene have been altered
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

          // We will draw our wn indicator -> sisable the default one
          ui->framesList->setDropIndicatorShown(false);

          if (index.isValid()) {
              // Catch overred item geometry
              QRect rect = ui->framesList->visualRect(index);

              // Compute mouse relative position
              int relativeX = pos.x() - rect.left();
              int width = rect.width();

              // DETECTION AREA
              // 20% TOLERANCY -> TOO MUCH ? May merge when insertion requested
              int margin = width * 0.2;

              if (relativeX < margin) {
                  // --- CASE : LEFT SIDE INSERTION ---
                  listDelegate->setHighlight(index.row(), FrameDelegate::InsertLeft);
                  clearMergeHighlight(); // not visible on atlas
                }
              else if (relativeX > (width - margin)) {
                  // --- CASE : RIGHT SIDE INSERTION ---
                  listDelegate->setHighlight(index.row(), FrameDelegate::InsertRight);
                  clearMergeHighlight(); // not visible on atlas
                }
              else {
                  // --- CASE : FUSION (CENTER) ---
                  listDelegate->setHighlight(index.row(), FrameDelegate::Merge);
                  setMergeHighlight(index, true); // shall be visible on atlas
                }
            } else {
              // empty space (start or end of the list)
              listDelegate->setHighlight(-1, FrameDelegate::None);
              clearMergeHighlight();
            }

          ui->framesList->viewport()->update();
          // Important : Let the drag continue
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

  // 1. Compute and merge
  Extractor::Box srcBox = extractor->m_atlas_index[sourceRow];
  Extractor::Box tgtBox = extractor->m_atlas_index[targetRow];

  QRect srcRect(srcBox.x, srcBox.y, srcBox.w, srcBox.h);
  QRect tgtRect(tgtBox.x, tgtBox.y, tgtBox.w, tgtBox.h);
  QRect unitedRect = srcRect.united(tgtRect);

  QPixmap mergedPixmap = this->extractor->m_atlas.copy(unitedRect);

  // 2. Update target internal data
  Extractor::Box newBox;
  newBox.x = unitedRect.x();
  newBox.y = unitedRect.y();
  newBox.w = unitedRect.width();
  newBox.h = unitedRect.height();

  extractor->m_atlas_index[targetRow] = newBox;
  extractor->m_frames[targetRow] = mergedPixmap;

  // 3. Update target visual
  QStandardItem *targetItem = frameModel->item(targetRow);
  if (targetItem) {
      QPixmap thumbnail = mergedPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      targetItem->setData(thumbnail, Qt::DecorationRole);
      // Make the merge operation visible on the list
      targetItem->setData(QString("%1 + %2 merge\n[%3,%4](%5x%6)")
                               .arg(sourceRow).arg(targetRow).arg(newBox.x).arg(newBox.y).arg(newBox.w).arg(newBox.h),
                           Qt::DisplayRole);
    }

  // 4. SOURCE DELETION
  extractor->m_atlas_index.removeAt(sourceRow);
  extractor->m_frames.removeAt(sourceRow);

  // Graphical cleanup
  if (boundingBoxHighlighter) {
      ui->graphicsViewLayers->scene()->removeItem(boundingBoxHighlighter);
      delete boundingBoxHighlighter;
      boundingBoxHighlighter = nullptr;
    }
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

  // Cleanup previous extractor
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

      // Check frames number
      int frameCount = movie.frameCount();

      if (frameCount > 1) {
          // CASE 1 : Animated GIF -> Use GifExtractor
          extractor = new GifExtractor(this);
        } else {
          // CASE 2 : non animated GIF (0 ou 1 frame) -> Use SpriteExtractor
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

             this->populateFrameList(extractor->m_frames, extractor->m_atlas_index);
             // Ensure animation is started after the loading
             this->setupGraphicsView(extractor->m_atlas);
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

void MainWindow::setupGraphicsView(const QPixmap &pixmap)
{
  // Get the existing scene or create a new one if it's the first time.
  QGraphicsScene *scene = ui->graphicsViewLayers->scene();
  if (!scene) {
      scene = new QGraphicsScene(this);
      ui->graphicsViewLayers->setScene(scene);
    }

  // Clear any previous items (old atlas, old bounding boxes) from the scene.
  scene->clear();

  // 1. Add the main atlas image to the scene.
  QGraphicsPixmapItem *atlasItem = new QGraphicsPixmapItem(pixmap);
  scene->addItem(atlasItem);

  // 2. Set the scene's bounding rect to the exact size of the atlas image.
  // This is crucial for fitInView() to know what dimensions to scale.
  scene->setSceneRect(pixmap.rect());

  // 3. Configure the view to display the entire scene, preserving aspect ratio.
  // This makes the atlas occupy the maximum space without cropping or distortion.
  // Qt::KeepAspectRatio ensures the image doesn't stretch to fill the view if the aspect ratios differ.
  ui->graphicsViewLayers->fitInView(atlasItem, Qt::KeepAspectRatio);

  // The animation preview view (graphicsViewResult) must also have its scene initialized,
  // although it will display individual frames later. We set its sceneRect to the max frame size.
  QGraphicsScene *resultScene = ui->graphicsViewResult->scene();
  if (!resultScene) {
      resultScene = new QGraphicsScene(this);
      ui->graphicsViewResult->setScene(resultScene);
    }

  // Set the scene size for the animation preview to the maximum bounding box size.
  // This prevents the frame from jumping around and maintains a fixed aspect ratio for the preview.
  if (extractor) {
      resultScene->setSceneRect(0, 0, extractor->m_maxFrameWidth, extractor->m_maxFrameHeight);
      // Force the result view to fit the scene rect.
      ui->graphicsViewResult->fitInView(resultScene->sceneRect(), Qt::KeepAspectRatio);
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
  // Call the base class implementation first.
  QMainWindow::resizeEvent(event);

  // Re-fit the atlas view (graphicsViewLayers)
  QGraphicsScene *atlasScene = ui->graphicsViewLayers->scene();
  if (atlasScene && !atlasScene->sceneRect().isEmpty()) {
      ui->graphicsViewLayers->fitInView(atlasScene->sceneRect(), Qt::KeepAspectRatio);
    }

  // Re-fit the animation preview view (graphicsViewResult)
  QGraphicsScene *resultScene = ui->graphicsViewResult->scene();
  if (resultScene && !resultScene->sceneRect().isEmpty()) {
      ui->graphicsViewResult->fitInView(resultScene->sceneRect(), Qt::KeepAspectRatio);
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
      return; // No scene or invalid index
    }

  QGraphicsScene *scene = ui->graphicsViewLayers->scene();
  const Extractor::Box &box = extractor->m_atlas_index.at(index.row());

  // 1. Remove existing highlighter
  if (boundingBoxHighlighter) {
      scene->removeItem(boundingBoxHighlighter);
      delete boundingBoxHighlighter;
      boundingBoxHighlighter = nullptr;
    }

  // 2. Create new rectangle (QGraphicsRectItem)
  // La Box est {x, y, w, h}
  QRectF rect(box.x, box.y, box.w, box.h);

  // 3. Drawing style definition
  QPen pen(Qt::red); // Use obvious color
  pen.setWidth(2);   // and a visible fat line
  pen.setStyle(Qt::DashLine); // Dashe line to increase visibility

  // 4. Draw the rectangle and add it to the scene
  boundingBoxHighlighter = new QGraphicsRectItem(rect);
  boundingBoxHighlighter->setPen(pen);
  boundingBoxHighlighter->setBrush(QBrush(QColor(255, 0, 0, 50))); // Fill with mid alpha

  scene->addItem(boundingBoxHighlighter);
}

void MainWindow::on_actionExport_triggered()
{
  // Check if have frames
  if (extractor->m_frames.isEmpty()) {
      QMessageBox::warning(this, tr("Exportation impossible"), tr("Veuillez charger ou créer des frames avant d'exporter."));
      return;
    }
  Extractor * extractorOut = nullptr;
  const QString filter = tr("Sprite Atlas (*.png *.json);;PNG Image (*.png)");

  QString selectedFile = QFileDialog::getSaveFileName(
      this,
      tr("Exporter le Sprite Atlas"),
      QDir::homePath(), // Default path TODO: remember previous if export is used multiple times
      filter
      );

  if (selectedFile.isEmpty()) {
      return; // Cancel case
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

