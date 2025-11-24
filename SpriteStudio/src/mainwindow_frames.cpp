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
  clearBoundingBoxHighlighters();
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
  clearBoundingBoxHighlighters();
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

void MainWindow::clearMergeHighlight()
{
  if (mergeHighlighter) {
      mergeHighlighter->setVisible(false);
    }
}

void MainWindow::populateFrameList(const QList<QPixmap> &frameList, const QList<Extractor::Box> &boxList)
{
  frameModel->clear();
  clearBoundingBoxHighlighters();

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
