#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include "include/config/appconfig.h"
#include <QMouseEvent>
#include <QDragMoveEvent>
#include <QMimeData>

void MainWindow::wheelEvent(QWheelEvent *event)
{
    QMainWindow::wheelEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (m_atlasController && m_document && !m_document->selectedFrameIndices().isEmpty()) {
        int key = event->key();
        if (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up || key == Qt::Key_Down) {
            const AtlasConfig &cfg = AppConfig::instance().atlas();
            int step = (event->modifiers() & Qt::ShiftModifier) ? cfg.nudgeStepLarge : cfg.nudgeStepSmall;
            int dx = 0;
            int dy = 0;
            if (key == Qt::Key_Left)  dx = -step;
            if (key == Qt::Key_Right) dx = step;
            if (key == Qt::Key_Up)    dy = -step;
            if (key == Qt::Key_Down)  dy = step;

            m_atlasController->nudgeSelectedBoxes(dx, dy);
            event->accept();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->framesList->viewport()) {
        if (event->type() == QEvent::DragMove) {
            QDragMoveEvent *dmEvent = static_cast<QDragMoveEvent *>(event);
            QPoint pos = dmEvent->position().toPoint();
            QModelIndex index = ui->framesList->indexAt(pos);

            ui->framesList->setDropIndicatorShown(false);

            if (index.isValid()) {
                QRect rect = ui->framesList->visualRect(index);
                int relativeX = pos.x() - rect.left();
                int width = rect.width();
                int margin = static_cast<int>(width * 0.2);

                if (relativeX < margin) {
                    listDelegate->setHighlight(index.row(), FrameDelegate::InsertLeft);
                } else if (relativeX > (width - margin)) {
                    listDelegate->setHighlight(index.row(), FrameDelegate::InsertRight);
                } else {
                    listDelegate->setHighlight(index.row(), FrameDelegate::Merge);
                }
            } else {
                listDelegate->setHighlight(-1, FrameDelegate::None);
            }

            ui->framesList->viewport()->update();
        } else if (event->type() == QEvent::DragLeave || event->type() == QEvent::Drop) {
            listDelegate->setHighlight(-1, FrameDelegate::None);
            ui->framesList->viewport()->update();
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    e->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_atlasController) {
        m_atlasController->adjustZoomToWindow();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            event->acceptProposedAction();
            return;
        }
    }
    QMainWindow::dragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString localFile = urls.first().toLocalFile();
        if (!localFile.isEmpty()) {
            processFile(localFile);
            event->acceptProposedAction();
            return;
        }
    }
    QMainWindow::dropEvent(event);
}
