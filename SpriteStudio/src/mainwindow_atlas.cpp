#include "include/mainwindow.h"
#include "./ui_mainwindow.h"
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

  if (!(selectionModifiers & (Qt::ControlModifier | Qt::ShiftModifier))) {
      ui->framesList->selectionModel()->clearSelection();
      clearBoundingBoxHighlighters();
    }

  selectionStartPoint = scenePos;
  isSelecting = true;
  ui->graphicsViewLayers->setCursor(Qt::CrossCursor);

  selectionRectItem = new QGraphicsRectItem();
  selectionRectItem->setPen(QPen(Qt::blue, 2, Qt::DashLine));
  selectionRectItem->setBrush(QBrush(QColor(0, 0, 255, 30)));
  ui->graphicsViewLayers->scene()->addItem(selectionRectItem);
}

void MainWindow::updateSelection(const QPointF &scenePos)
{
  if (!isSelecting || !selectionRectItem) return;

  QRectF rect(selectionStartPoint, scenePos);
  selectionRectItem->setRect(rect.normalized());
}

void MainWindow::endSelection()
{
  if (!isSelecting || !selectionRectItem) return;

  QRectF selectionRect = selectionRectItem->rect();
  QList<int> selectedFrameIndices = findFramesInSelectionRect(selectionRect);

  if (!selectedFrameIndices.isEmpty()) {
      selectFramesInList(selectedFrameIndices);
    }

  ui->graphicsViewLayers->scene()->removeItem(selectionRectItem);
  delete selectionRectItem;
  selectionRectItem = nullptr;
  isSelecting = false;
  ui->graphicsViewLayers->setCursor(Qt::ArrowCursor);
}

QList<int> MainWindow::findFramesInSelectionRect(const QRectF &rect)
{
  QList<int> result;
  if (!extractor) return result;

  for (int i = 0; i < extractor->m_atlas_index.size(); ++i) {
      const Extractor::Box &box = extractor->m_atlas_index.at(i);
      QRectF frameRect(box.x, box.y, box.w, box.h);

      if (rect.intersects(frameRect)) {
          result.append(i);
        }
    }
  return result;
}

void MainWindow::selectFramesInList(const QList<int> &frameIndices)
{
  if (!frameModel || frameIndices.isEmpty()) return;

  QItemSelectionModel *selectionModel = ui->framesList->selectionModel();
  if (!selectionModel) return;

  bool addToSelection = (selectionModifiers & (Qt::ControlModifier | Qt::ShiftModifier));

  if (!addToSelection) {
      selectionModel->clearSelection();
    }

  QItemSelection selection;
  for (int row : frameIndices) {
      QModelIndex index = frameModel->index(row, 0);
      if (index.isValid()) {
          selection.select(index, index);
        }
    }

  if (selectionModifiers & Qt::ControlModifier) {
      for (int row : frameIndices) {
          QModelIndex index = frameModel->index(row, 0);
          if (selectionModel->isSelected(index)) {
              selectionModel->select(index, QItemSelectionModel::Deselect);
            } else {
              selectionModel->select(index, QItemSelectionModel::Select);
            }
        }
    } else {
      selectionModel->select(selection, QItemSelectionModel::Select);
    }

  on_framesList_clicked(QModelIndex());
}
