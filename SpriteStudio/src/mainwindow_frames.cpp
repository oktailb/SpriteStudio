#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include <QStandardItem>

void MainWindow::populateFrameList(const QList<QPixmap> &frameList, const QList<SpriteBox> &boxList)
{
    frameModel->clear();
    frameModel->setColumnCount(1);

    int itemCount = qMin(frameList.size(), boxList.size());
    if (progressBar) progressBar->setValue(0);
    if (statusLabel) statusLabel->setText(tr("_populating_frame_list"));

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

        if (progressBar && itemCount > 0) {
            progressBar->setValue(100 * i / itemCount);
        }
    }

    if (progressBar) progressBar->setValue(0);
    if (statusLabel) statusLabel->setText(tr("_ready"));
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
