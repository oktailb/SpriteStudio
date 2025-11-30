#include "include/mainwindow.h"
#include "./ui_mainwindow.h"
#include "qfiledialog.h"
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

void MainWindow::wheelEvent(QWheelEvent *event)
{
  if (event->modifiers() & Qt::ControlModifier) {
      QGraphicsView *view = ui->graphicsViewLayers;
      double factor = (event->angleDelta().y() > 0) ? 1.1 : 0.9;
      view->scale(factor, factor);
      event->accept();
    } else {
      QMainWindow::wheelEvent(event);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == ui->graphicsViewLayers->viewport()) {
      QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

      if (event->type() == QEvent::MouseButtonPress) {
          if (mouseEvent->button() == Qt::LeftButton && extractor) {
              QPointF scenePos = ui->graphicsViewLayers->mapToScene(mouseEvent->pos());
              startSelection(scenePos);
              return true;
            }
        }
      else if (event->type() == QEvent::MouseMove) {
          if (isSelecting) {
              QPointF scenePos = ui->graphicsViewLayers->mapToScene(mouseEvent->pos());
              updateSelection(scenePos);
              return true;
            }
        }
      else if (event->type() == QEvent::MouseButtonRelease) {
          if (mouseEvent->button() == Qt::LeftButton && isSelecting) {
              endSelection();
              return true;
            }
        }
    }

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
                  // listDelegate->setHighlight(index.row(), FrameDelegate::InsertLeft);
                  // clearMergeHighlight(); // not visible on atlas
                }
              else if (relativeX > (width - margin)) {
                  // --- CASE : RIGHT SIDE INSERTION ---
                  // listDelegate->setHighlight(index.row(), FrameDelegate::InsertRight);
                  // clearMergeHighlight(); // not visible on atlas
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
        }
      else if (event->type() == QEvent::DragLeave || event->type() == QEvent::Drop) {
          listDelegate->setHighlight(-1, FrameDelegate::None);
          clearMergeHighlight();
          ui->framesList->viewport()->update();
        }
    }

  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
  Q_UNUSED(e);
}

void MainWindow::timerEvent(QTimerEvent *event)
{
  Q_UNUSED(event);
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
