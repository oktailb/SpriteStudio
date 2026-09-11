#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include <QMenu>
#include <QAction>

void MainWindow::on_Play_clicked()
{
    if (m_animationController) {
        m_animationController->play();
    }
}

void MainWindow::on_Pause_clicked()
{
    if (m_animationController) {
        m_animationController->pause();
    }
}

void MainWindow::on_fps_valueChanged(int fps)
{
    if (m_animationController) {
        m_animationController->setFps(fps);
    }
}

void MainWindow::on_animationList_customContextMenuRequested(const QPoint &pos)
{
    QMenu menu(this);

    QAction *createAnimAction = menu.addAction(tr("Create animation from selection"));
    createAnimAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(createAnimAction, &QAction::triggered, this, [this]() {
        if (m_animationController && m_document) {
            m_animationController->createAnimationFromSelection(m_document->selectedFrameIndices());
        }
    });

    QAction *reverseAction = menu.addAction(tr("Reverse Animation"));
    connect(reverseAction, &QAction::triggered, this, [this]() {
        if (m_animationController) m_animationController->reverseAnimationOrder();
    });

    QAction *deleteAction = menu.addAction(tr("Delete Animation"));
    connect(deleteAction, &QAction::triggered, this, [this]() {
        if (m_animationController) m_animationController->removeSelectedAnimation();
    });

    menu.exec(ui->animationList->mapToGlobal(pos));
}
