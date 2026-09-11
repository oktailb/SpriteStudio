#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include <QStandardItem>

void MainWindow::populateFrameList(const QList<QPixmap> &frameList, const QList<SpriteBox> &boxList)
{
    m_isSyncingSelection = true;
    frameModel->clear();
    frameModel->setColumnCount(1);

    int itemCount = qMin(frameList.size(), boxList.size());
    if (progressBar) progressBar->setValue(0);
    if (statusLabel) statusLabel->setText(tr("KEY_STATUS_POPULATING"));

    for (int i = 0; i < itemCount; ++i) {
        const QPixmap &pixmap = frameList.at(i);
        const SpriteBox &box = boxList.at(i);

        QStandardItem *item = new QStandardItem();
        QPixmap thumbnail = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        item->setData(thumbnail, Qt::DecorationRole);

        QString displayText = tr("KEY_FRAME_LABEL").arg(i + 1);
        if (box.selected) {
            displayText += " ✓";
            item->setBackground(QBrush(QColor(200, 230, 255)));
        }

        item->setData(displayText, Qt::DisplayRole);
        item->setData(i, Qt::UserRole);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsEditable);
        frameModel->appendRow(item);

        if (progressBar && itemCount > 0) {
            progressBar->setValue(100 * i / itemCount);
        }
    }

    // Re-apply selection to ui->framesList if document has active selections
    if (m_document && ui->framesList->selectionModel()) {
        QItemSelection sel;
        for (int row : m_document->selectedFrameIndices()) {
            QModelIndex mIdx = frameModel->index(row, 0);
            if (mIdx.isValid()) {
                sel.select(mIdx, mIdx);
            }
        }
        ui->framesList->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
    }
    m_isSyncingSelection = false;

    if (progressBar) progressBar->setValue(0);
    if (statusLabel) statusLabel->setText(tr("KEY_STATUS_READY"));
}

void MainWindow::refreshFrameListDisplay()
{
    if (!frameModel || !m_document) return;
    int count = qMin(frameModel->rowCount(), m_document->frameCount());
    for (int i = 0; i < count; ++i) {
        QStandardItem *item = frameModel->item(i);
        if (!item) continue;
        bool isSel = m_document->box(i).selected;
        QString base = tr("KEY_FRAME_LABEL").arg(i + 1);
        QString text = QString("%1%2").arg(base).arg(isSel ? " ✓" : "");
        item->setData(text, Qt::DisplayRole);
        if (isSel) {
            item->setBackground(QBrush(QColor(200, 230, 255)));
        } else {
            item->setData(QVariant(), Qt::BackgroundRole);
        }
    }
}

void MainWindow::syncFromDocument()
{
    if (!m_document) return;

    populateFrameList(m_document->frames(), m_document->boxes());
    if (m_atlasController) {
        m_atlasController->setAtlasImage(m_document->atlas());
    }
    if (m_animationController) {
        m_animationController->syncAnimationList();
        if (!m_document->animations().isEmpty()) {
            m_animationController->selectAnimation(m_document->animations().firstKey());
        }
    }
}
