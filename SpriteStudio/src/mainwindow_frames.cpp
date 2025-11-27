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
    QList<int> selectedIndices = getSelectedFrameIndices();

    if (selectedIndices.isEmpty()) return;

    reverseFramesOrder(selectedIndices);
}

void MainWindow::reverseFramesOrder(const QList<int> &selectedIndices)
{
    if (selectedIndices.isEmpty()) return;

    QList<int> sortedIndices = selectedIndices;
    std::sort(sortedIndices.begin(), sortedIndices.end());

    int firstRow = sortedIndices.first();
    int lastRow = sortedIndices.last();

    if (firstRow == lastRow) return;

    // Créer le nouvel ordre
    QList<int> newOrder;
    for (int i = 0; i < extractor->m_frames.size(); ++i) {
        if (i < firstRow || i > lastRow) {
            newOrder.append(i);
        }
    }

    // Ajouter la sélection inversée
    for (int i = lastRow; i >= firstRow; --i) {
        newOrder.insert(firstRow, i);
    }

    extractor->reorderFrames(newOrder);

    populateFrameList(extractor->m_frames, extractor->m_atlas_index);
    syncAnimationListWidget();
    startAnimation();
}

void MainWindow::invertSelection()
{
    if (!extractor) return;

    invertFrameSelection();
}

void MainWindow::invertFrameSelection()
{
    if (!extractor) return;

    QList<int> newSelection;
    for (int i = 0; i < extractor->m_atlas_index.size(); ++i) {
        if (!extractor->m_atlas_index.at(i).selected) {
            newSelection.append(i);
        }
    }

    setSelectedFrameIndices(newSelection);

    // Mettre à jour les surbrillances
    clearBoundingBoxHighlighters();
    setBoundingBoxHighllithers(newSelection);
}

void MainWindow::deleteFrame(int row)
{
    if (row < 0 || row >= extractor->m_atlas_index.size()) return;

    // Utiliser la nouvelle méthode Extractor
    extractor->removeFrame(row);

    // Mettre à jour le modèle d'affichage
    frameModel->removeRow(row);

    // Synchroniser les animations
    syncAnimationListWidget();

    // Nettoyer l'affichage
    clearBoundingBoxHighlighters();
}

void MainWindow::deleteSelectedFrame()
{
    QList<int> selectedIndices = getSelectedFrameIndices();

    if (selectedIndices.isEmpty()) return;

    deleteFrames(selectedIndices);
}

void MainWindow::deleteFrames(const QList<int> &frameIndices)
{
    if (frameIndices.isEmpty() || !extractor) return;

    // Trier par ordre décroissant pour suppression sécurisée
    QList<int> sortedIndices = frameIndices;
    std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());

    // Supprimer de l'extracteur
    for (int rowToDelete : sortedIndices) {
        extractor->removeFrame(rowToDelete);
    }

    // Reconstruire l'affichage
    populateFrameList(extractor->m_frames, extractor->m_atlas_index);

    // Nettoyer
    clearBoundingBoxHighlighters();

    // Mettre à jour l'animation "current" (elle sera recalculée automatiquement)
    updateCurrentAnimation();

    // Arrêter l'animation si nécessaire
    stopAnimation();
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

    // S'assurer que les deux listes ont la même taille
    if (frameList.size() != boxList.size()) {
        qWarning() << "Incohérence dans populateFrameList: frames =" << frameList.size()
        << ", boxes =" << boxList.size();
    }

    int itemCount = qMin(frameList.size(), boxList.size());

    for (int i = 0; i < itemCount; ++i) {
        const QPixmap &pixmap = frameList.at(i);
        const Extractor::Box &box = boxList.at(i);

        QStandardItem *item = new QStandardItem();

        QPixmap thumbnail = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        item->setData(thumbnail, Qt::DecorationRole);

        // Ajouter un indicateur visuel pour les frames sélectionnées
        QString displayText = QString("Frame %1 on %2").arg(i + 1).arg(itemCount);
        if (box.selected) {
            displayText += " ✓";
            item->setBackground(QBrush(QColor(200, 230, 255)));
        }

        item->setData(displayText, Qt::DisplayRole);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        frameModel->appendRow(item);
    }

    syncAnimationListWidget();

    qDebug() << "Frame list populated with" << itemCount << "items";
}

QList<int> MainWindow::getSelectedFrameIndices() const
{
    QList<int> selectedIndices;
    if (!extractor) return selectedIndices;

    for (int i = 0; i < extractor->m_atlas_index.size(); ++i) {
        if (extractor->m_atlas_index.at(i).selected) {
            selectedIndices.append(i);
        }
    }
    return selectedIndices;
}

void MainWindow::setSelectedFrameIndices(const QList<int> &selectedIndices)
{
    if (!extractor) return;

    // Réinitialiser toutes les sélections
    clearFrameSelections();

    // Marquer les nouvelles sélections
    for (int index : selectedIndices) {
        if (index >= 0 && index < extractor->m_atlas_index.size()) {
            extractor->m_atlas_index[index].selected = true;
        }
    }

    // Mettre à jour l'affichage
    updateFrameListSelectionFromModel();

    // METTRE À JOUR l'animation "current" (gère aussi le cas vide)
    updateCurrentAnimation();
}

void MainWindow::clearFrameSelections()
{
    if (!extractor) return;

    for (Extractor::Box &box : extractor->m_atlas_index) {
        box.selected = false;
    }

    updateFrameListSelectionFromModel();

    // METTRE À JOUR l'animation "current" (va la supprimer car sélection vide)
    updateCurrentAnimation();
}

void MainWindow::updateFrameListSelectionFromModel()
{
    // if (!extractor || !ui->framesList->selectionModel()) return;

    // // Bloquer les signaux pour éviter les boucles
    // ui->framesList->blockSignals(true);

    // QItemSelectionModel *selectionModel = ui->framesList->selectionModel();
    // selectionModel->clearSelection();

    // QItemSelection selection;
    // for (int i = 0; i < extractor->m_atlas_index.size(); ++i) {
    //     if (extractor->m_atlas_index.at(i).selected) {
    //         QModelIndex index = frameModel->index(i, 0);
    //         if (index.isValid()) {
    //             selection.select(index, index);
    //         }
    //     }
    // }

    // selectionModel->select(selection, QItemSelectionModel::Select);
    // ui->framesList->blockSignals(false);

    // // Rafraîchir l'affichage
    // refreshFrameListDisplay();
}
