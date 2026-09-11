#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include <QMenu>
#include <QAction>

void MainWindow::on_btnToolSelect_clicked()
{
    if (m_atlasController) {
        m_atlasController->setToolMode(AtlasViewController::ToolSelect);
    }
}

void MainWindow::on_btnToolAddSlice_clicked()
{
    if (m_atlasController) {
        m_atlasController->setToolMode(AtlasViewController::ToolAddSlice);
    }
}

void MainWindow::on_btnTrimSlice_clicked()
{
    if (m_atlasController) {
        m_atlasController->trimSelectedSlice(ui->alphaThreshold ? ui->alphaThreshold->value() : 1);
    }
}

void MainWindow::onBoxContextMenuRequested(int index, const QPoint &screenPos)
{
    if (!m_document || index < 0 || index >= m_document->frameCount()) return;

    QList<int> currentSel = m_document->selectedFrameIndices();
    if (!currentSel.contains(index)) {
        m_atlasController->setSelectedBoxIndices({index});
        currentSel = {index};
    }

    QMenu menu(this);

    QAction *createAnimAction = menu.addAction(tr("Create animation from selection"));
    createAnimAction->setEnabled(!currentSel.isEmpty());
    connect(createAnimAction, &QAction::triggered, this, [this, currentSel]() {
        if (m_animationController) {
            m_animationController->createAnimationFromSelection(currentSel);
        }
    });

    menu.addSeparator();

    QAction *trimAction = menu.addAction(tr("Trim to Pixels / Ajuster aux pixels"));
    connect(trimAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->trimSelectedSlice(ui->alphaThreshold ? ui->alphaThreshold->value() : 1);
    });

    QAction *mergeAction = menu.addAction(tr("Merge Slices / Fusionner les boîtes"));
    mergeAction->setEnabled(currentSel.size() >= 2);
    connect(mergeAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->mergeSelectedSlices();
    });

    menu.addSeparator();

    QAction *deleteAction = menu.addAction(tr("Delete Slice / Supprimer"));
    connect(deleteAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->deleteSelectedSlices();
    });

    menu.addSeparator();

    QAction *removeBgAction = menu.addAction(tr("Auto Remove Background / Supprimer l'arrière-plan"));
    removeBgAction->setEnabled(m_document && !m_document->atlas().isNull());
    connect(removeBgAction, &QAction::triggered, this, &MainWindow::removeAtlasBackgroundAndRefresh);

    menu.exec(screenPos);
}

void MainWindow::onAtlasContextMenuRequested(const QPoint &pos)
{
    QMenu menu(this);

    QAction *createAnimAction = menu.addAction(tr("Create animation from selection"));
    createAnimAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(createAnimAction, &QAction::triggered, this, [this]() {
        if (m_animationController && m_document) {
            m_animationController->createAnimationFromSelection(m_document->selectedFrameIndices());
        }
    });

    QAction *trimAction = menu.addAction(tr("Trim to Pixels"));
    trimAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(trimAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->trimSelectedSlice(ui->alphaThreshold ? ui->alphaThreshold->value() : 1);
    });

    QAction *mergeSlicesAction = menu.addAction(tr("Merge Slices"));
    mergeSlicesAction->setEnabled(m_document && m_document->selectedFrameIndices().size() >= 2);
    connect(mergeSlicesAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->mergeSelectedSlices();
    });

    menu.addSeparator();

    QAction *deleteFramesAction = menu.addAction(tr("Delete Selected Frames"));
    deleteFramesAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(deleteFramesAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->deleteSelectedSlices();
    });

    QAction *invertAction = menu.addAction(tr("Invert Selection"));
    invertAction->setEnabled(m_document && m_document->frameCount() > 0);
    connect(invertAction, &QAction::triggered, this, &MainWindow::invertSelection);

    menu.addSeparator();

    QAction *removeBgAction = menu.addAction(tr("Auto Remove Background / Supprimer l'arrière-plan"));
    removeBgAction->setEnabled(m_document && !m_document->atlas().isNull());
    connect(removeBgAction, &QAction::triggered, this, &MainWindow::removeAtlasBackgroundAndRefresh);

    QPoint globalPos = (ui->graphicsViewLayers && ui->graphicsViewLayers->viewport())
        ? ui->graphicsViewLayers->viewport()->mapToGlobal(pos)
        : pos;
    menu.exec(globalPos);
}

void MainWindow::removeAtlasBackgroundAndRefresh()
{
    if (m_projectController) {
        m_projectController->removeAtlasBackgroundAndRefresh(
            ui->alphaThreshold ? ui->alphaThreshold->value() : 10,
            ui->verticalTolerance ? ui->verticalTolerance->value() : 5,
            ui->enableSmartCropCheckbox ? ui->enableSmartCropCheckbox->isChecked() : false,
            ui->overlapThresholdSpinbox ? ui->overlapThresholdSpinbox->value() : 0.5
        );
    }
}
