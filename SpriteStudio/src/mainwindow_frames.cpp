#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include <algorithm>

void MainWindow::reverseFramesOrder(QList<int> &selectedIndices)
{
    if (selectedIndices.isEmpty()) return;

    std::reverse(selectedIndices.begin(), selectedIndices.end());
    syncAnimationListWidget();
    startAnimation();
}

void MainWindow::invertSelection()
{
    invertFrameSelection();
}

void MainWindow::invertFrameSelection()
{
    if (!m_document) return;

    QList<int> newSelection;
    for (int i = 0; i < m_document->boxes().size(); ++i) {
        if (!m_document->box(i).selected) {
            newSelection.append(i);
        }
    }

    setSelectedFrameIndices(newSelection);

    clearBoundingBoxHighlighters();
    setBoundingBoxHighlighters(newSelection);
}

void MainWindow::deleteFrame(int row)
{
    if (!m_document || row < 0 || row >= m_document->frameCount()) return;

    m_undoStack->push(new DeleteFramesCommand(m_document, {row}));
}

void MainWindow::deleteSelectedFrame()
{
    QList<int> selectedIndices = getSelectedFrameIndices();
    if (selectedIndices.isEmpty()) return;

    deleteFrames(selectedIndices);
}

void MainWindow::deleteFrames(const QList<int> &frameIndices)
{
    if (!m_document || frameIndices.isEmpty()) return;

    m_undoStack->push(new DeleteFramesCommand(m_document, frameIndices));
}

void MainWindow::setMergeHighlight(const QModelIndex &index, bool show)
{
    if (!show || !index.isValid() || !m_document) {
        clearMergeHighlight();
        return;
    }

    int row = index.row();
    if (row >= 0 && row < m_document->boxes().size()) {
        QRectF rect(m_document->box(row).rect);

        if (!mergeHighlighter) {
            mergeHighlighter = new QGraphicsRectItem();
            QPen pen(Qt::magenta);
            pen.setWidth(3);
            pen.setStyle(Qt::DotLine);
            mergeHighlighter->setPen(pen);
            mergeHighlighter->setZValue(10);

            if (ui->graphicsViewLayers->scene()) {
                ui->graphicsViewLayers->scene()->addItem(mergeHighlighter);
            }
        }

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

void MainWindow::populateFrameList(const QList<QPixmap> &frameList, const QList<SpriteBox> &boxList)
{
    frameModel->clear();
    clearBoundingBoxHighlighters();

    frameModel->setColumnCount(1);

    int itemCount = qMin(frameList.size(), boxList.size());
    progressBar->setValue(0);
    statusLabel->setText(tr("_populating_frame_list"));

    for (int i = 0; i < itemCount; ++i) {
        const QPixmap &pixmap = frameList.at(i);
        const SpriteBox &box = boxList.at(i);

        QStandardItem *item = new QStandardItem();
        QPixmap thumbnail = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        item->setData(thumbnail, Qt::DecorationRole);

        QString displayText = QString("Frame %1").arg(i + 1);
        if (box.selected) {
            displayText += " ✓";
            item->setBackground(QBrush(QColor(200, 230, 255)));
        }

        item->setData(displayText, Qt::DisplayRole);
        item->setData(i, Qt::UserRole);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsEditable);
        frameModel->appendRow(item);
        if (itemCount > 0) {
            progressBar->setValue(100 * i / itemCount);
        }
    }
    progressBar->setValue(0);
    statusLabel->setText(tr("_ready"));

    syncAnimationListWidget();
}

QList<int> MainWindow::getSelectedFrameIndices() const
{
    return m_document ? m_document->selectedFrameIndices() : QList<int>();
}

void MainWindow::setSelectedFrameIndices(const QList<int> &selectedIndices)
{
    if (!m_document) return;

    m_document->clearBoxSelections();
    for (int index : selectedIndices) {
        m_document->setBoxSelection(index, true);
    }

    updateFrameListSelectionFromModel();
    updateCurrentAnimation();
}

void MainWindow::clearFrameSelections()
{
    if (!m_document) return;

    m_document->clearBoxSelections();
    updateFrameListSelectionFromModel();
    updateCurrentAnimation();
}

void MainWindow::updateFrameListSelectionFromModel()
{
    if (!m_document || !frameModel) return;

    QItemSelection selection;
    for (int idx : m_document->selectedFrameIndices()) {
        QModelIndex index = frameModel->index(idx, 0);
        if (index.isValid()) {
            selection.select(index, index);
        }
    }
    ui->framesList->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
}

void MainWindow::syncFromDocument()
{
    if (!m_document) return;

    populateFrameList(m_document->frames(), m_document->boxes());
    setupGraphicsView(m_document->atlas());
    syncAnimationListWidget();

    if (!m_document->animations().isEmpty()) {
        QString firstAnim = m_document->animations().firstKey();
        QList<QTreeWidgetItem*> items = ui->animationList->findItems(firstAnim, Qt::MatchExactly, 0);
        if (!items.isEmpty()) {
            ui->animationList->setCurrentItem(items.first());
            startAnimation();
        }
    }
}
