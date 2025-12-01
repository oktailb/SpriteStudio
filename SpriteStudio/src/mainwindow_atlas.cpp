#include "include/mainwindow.h"
#include "./ui_mainwindow.h"
#include "ui_mainwindow.h"
#include <stdfloat>
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
          QMessageBox::critical(this, tr("_file_error"), tr("_gif_error"));
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
      SpriteExtractor *tmp = static_cast<SpriteExtractor*>(extractor);
      tmp->setSmartCropEnabled(ui->enableSmartCropCheckbox->isEnabled());
      tmp->setOverlapThreshold(ui->overlapThresholdSpinbox->value());
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
  clearBoundingBoxHighlighters();
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

void MainWindow::clearBoundingBoxHighlighters()
{
    for (QGraphicsRectItem *highlighter : boundingBoxHighlighters) {
        if (highlighter && highlighter->scene()) {
            highlighter->scene()->removeItem(highlighter);
            delete highlighter;
        }
    }
    boundingBoxHighlighters.clear();
}

void MainWindow::setBoundingBoxHighllithers(const QList<int> &selectedIndices)
{
    QGraphicsScene *scene = ui->graphicsViewLayers->scene();
    if (!scene || !extractor) return;

    for (int i = 0; i < selectedIndices.size(); ++i) {
        int row = selectedIndices.at(i);

        if (row < 0 || row >= extractor->m_atlas_index.size()) {
            qWarning() << "Index de frame invalide dans setBoundingBoxHighllithers:" << row;
            continue;
        }

        const Extractor::Box &box = extractor->m_atlas_index[row];
        QRectF rect(box.rect);
        QColor color = getHighlightColor(i, selectedIndices.size());
        color.setAlpha(50);
        QPen pen(Qt::red);
        pen.setWidth(2);
        pen.setStyle(Qt::DashLine);

        QGraphicsRectItem *highlighter = new QGraphicsRectItem(rect);
        highlighter->setPen(pen);
        highlighter->setBrush(QBrush(color));
        highlighter->setToolTip(QString("Frame %1 on %2").arg(row + 1).arg(selectedIndices.size()));

        QGraphicsSimpleTextItem *label = new QGraphicsSimpleTextItem(QString::number(row + 1), highlighter);
        label->setPos(rect.topLeft());
        label->setBrush(Qt::black);
        label->setFont(QFont("Arial", 10, QFont::Bold));

        scene->addItem(highlighter);
        boundingBoxHighlighters.append(highlighter);
        if (!isSelecting)
          fitSelectedFramesInView(100);
    }
}

void MainWindow::refreshFrameListDisplay()
{
    if (!extractor) return;

    int modelCount = frameModel->rowCount();
    int atlasCount = extractor->m_atlas_index.size();

    if (modelCount != atlasCount) {
        qWarning() << "Incohérence détectée: frameModel a" << modelCount
                   << "items, mais extractor a" << atlasCount << "frames";
        populateFrameList(extractor->m_frames, extractor->m_atlas_index);
        return;
    }

    for (int i = 0; i < modelCount; ++i) {
        QStandardItem *item = frameModel->item(i, 0);
        if (!item) continue;

        if (i >= extractor->m_atlas_index.size()) {
            qWarning() << "Index" << i << "hors limites dans refreshFrameListDisplay()";
            break;
        }

        const Extractor::Box &box = extractor->m_atlas_index.at(i);

        QString displayText = QString("Frame ") + QString::number(i) + " on " + QString::number(modelCount);
        if (box.selected) {
            displayText += " ✓";
            item->setBackground(QBrush(Qt::magenta));
        } else {
            item->setBackground(QBrush());
        }
        item->setTextAlignment(Qt::AlignBaseline);
        item->setData(displayText, Qt::ToolTipRole);
        item->setData(i, Qt::UserRole);
    }
}

void MainWindow::fitSelectedFramesInView(int padding)
{
  if (boundingBoxHighlighters.isEmpty() || !extractor) return;

  QRectF unitedRect;
  for (QGraphicsRectItem *highlighter : boundingBoxHighlighters) {
      if (unitedRect.isNull()) {
          unitedRect = highlighter->rect();
        } else {
          unitedRect = unitedRect.united(highlighter->rect());
        }
    }

  unitedRect.adjust(-padding, -padding, padding, padding); // enought large to facilitate next selction

  ui->graphicsViewLayers->fitInView(unitedRect, Qt::KeepAspectRatio);
}

void MainWindow::startSelection(const QPointF &scenePos)
{
  if (!extractor || extractor->m_atlas_index.isEmpty()) return;

  selectionModifiers = QApplication::keyboardModifiers();

         // Initialize currentSelection based on modifiers
  if (!(selectionModifiers & (Qt::ControlModifier | Qt::ShiftModifier))) {
      // No modifier: start with empty selection
      currentSelection.clear();
    }

  selectionStartPoint = scenePos;
  isSelecting = true;
  ui->graphicsViewLayers->setCursor(Qt::CrossCursor);

  selectionRectItem = new QGraphicsRectItem();
  selectionRectItem->setPen(QPen(Qt::blue, 2, Qt::DashLine));
  selectionRectItem->setBrush(QBrush(QColor(0, 0, 255, 30)));
  ui->graphicsViewLayers->scene()->addItem(selectionRectItem);

  // Update immediately to show initial selection
  updateSelection(scenePos);
}

void MainWindow::updateSelection(const QPointF &scenePos)
{
  if (!isSelecting || !selectionRectItem) return;

  QRectF rect(selectionStartPoint, scenePos);
  selectionRectItem->setRect(rect.normalized());

  QList<int> selectedFrameIndices = findFramesInSelectionRect(rect);

  QList<int> newSelection;

  if (selectionModifiers & Qt::ControlModifier) {
      // CTRL: Add frames to existing selection
      newSelection = currentSelection; // Start with current selection

      // Add only new frames that aren't already selected
      for (int index : selectedFrameIndices) {
          if (!newSelection.contains(index)) {
              newSelection.append(index);
            }
        }
    } else if (selectionModifiers & Qt::ShiftModifier) {
      // SHIFT: Remove frames from existing selection
      newSelection = currentSelection; // Start with current selection

      // Remove frames that are in the current selection rectangle
      for (int index : selectedFrameIndices) {
          newSelection.removeAll(index);
        }
    } else {
      // No modifier: Replace selection - PRESERVE ORDER from findFramesInSelectionRect
      // Add only new frames that aren't already selected
      for (int index : selectedFrameIndices) {
          if (!currentSelection.contains(index)) {
              currentSelection.append(index);
            }
        }
      newSelection = currentSelection;
    }

         // Update current selection
  currentSelection = newSelection;

         // Update visual feedback
  clearBoundingBoxHighlighters();
  setBoundingBoxHighllithers(currentSelection);
  updateCurrentAnimation();

}

void MainWindow::endSelection()
{
  if (!isSelecting || !selectionRectItem) return;

  if (!currentSelection.isEmpty()) {
      // 1. Clear existing selections in extractor
      clearFrameSelections();

      // 2. Set new selections in extractor
      for (int index : currentSelection) {
          if (index >= 0 && index < extractor->m_atlas_index.size()) {
              extractor->m_atlas_index[index].selected = true;
            }
        }

      // 3. Update all UI components
      clearBoundingBoxHighlighters();
      setBoundingBoxHighllithers(currentSelection);

      // 4. Animation is already updated by updateCurrentAnimation() in updateSelection()
      // Just ensure the animation list is synced
      syncAnimationListWidget();

      // 5. Update list view selection
      QItemSelection selection;
      for (int row : currentSelection) {
          QModelIndex index = frameModel->index(row, 0);
          if (index.isValid()) {
              selection.select(index, index);
            }
        }

      ui->framesList->blockSignals(true);
      ui->framesList->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
      ui->framesList->blockSignals(false);

      // 6. Refresh the frame list display
      refreshFrameListDisplay();

    } else {
      removeCurrentAnimation();
    }

         // Cleanup
  ui->graphicsViewLayers->scene()->removeItem(selectionRectItem);
  delete selectionRectItem;
  selectionRectItem = nullptr;
  isSelecting = false;
  ui->graphicsViewLayers->setCursor(Qt::ArrowCursor);
  startAnimation();
}

QList<int> MainWindow::findFramesInSelectionRect(const QRectF &rect)
{
  QList<int> result;
  if (!extractor) return result;

         // Return frames in the order they are found (no sorting)
         // This preserves the natural selection order
  for (int i = 0; i < extractor->m_atlas_index.size(); ++i) {
      const Extractor::Box &box = extractor->m_atlas_index.at(i);
      QRectF frameRect(box.rect);

      if (rect.intersects(frameRect)) {
          result.append(i);
        }
    }

  // No sorting - preserve the natural order of discovery
  return result;
}

void MainWindow::selectFramesInList(const QList<int> &frameIndices)
{
    if (!frameModel || frameIndices.isEmpty()) return;

    setSelectedFrameIndices(frameIndices);

    clearBoundingBoxHighlighters();
    setBoundingBoxHighllithers(frameIndices);

    updateCurrentAnimation();
}

void MainWindow::deleteSelectedFramesFromAtlas()
{
    if (!extractor || extractor->m_frames.isEmpty()) return;

    QList<int> selectedIndices = getSelectedFrameIndices();
    if (selectedIndices.isEmpty()) {
        QMessageBox::information(this, tr("_info"), tr("_select_frames_first"));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("_confirm_delete"),
        tr("_confirm_delete_frames").arg(selectedIndices.size()),
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply != QMessageBox::Yes) {
        return;
    }

    QList<QRect> areasToClear;
    for (int index : selectedIndices) {
        if (index >= 0 && index < extractor->m_atlas_index.size()) {
            const Extractor::Box &box = extractor->m_atlas_index.at(index);
            areasToClear.append(box.rect);
        }
    }

    extractor->clearAtlasAreas(selectedIndices);
    extractor->removeFrames(selectedIndices);
    populateFrameList(extractor->m_frames, extractor->m_atlas_index);
    syncAnimationListWidget();
    setupGraphicsView(extractor->m_atlas);
    stopAnimation();
    clearBoundingBoxHighlighters();

    QMessageBox::information(this, tr("_success"),
                             tr("_frames_deleted").arg(selectedIndices.size()));
}

