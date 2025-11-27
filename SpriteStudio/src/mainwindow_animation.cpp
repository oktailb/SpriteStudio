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

void MainWindow::createAnimationFromSelection()
{
    QList<int> selectedIndices = getSelectedFrameIndices();

    if (selectedIndices.isEmpty()) return;

    bool ok;
    QString text = QInputDialog::getText(this, tr("_new_animation"),
                                         tr("_animation_name"), QLineEdit::Normal,
                                         tr("_new_animation"), &ok);

    if (!ok || text.isEmpty()) return;

    int currentFps = ui->fps->value();

    createAnimation(text, selectedIndices, currentFps);
}

void MainWindow::createAnimation(QString name, QList<int> selectedIndices, int fps)
{
    // Pour l'animation "current", s'assurer qu'elle est toujours à jour
    if (name == "current") {
        extractor->setAnimation(name, selectedIndices, fps);

        // Ne pas sélectionner automatiquement l'animation "current"
        // et ne pas lancer l'animation automatiquement
        updateAnimationsList();

        // Redimensionner les colonnes
        for (int c = 0; c < ui->animationList->columnCount(); c++) {
            ui->animationList->resizeColumnToContents(c);
        }
        return;
    }

    // Comportement normal pour les autres animations
    extractor->setAnimation(name, selectedIndices, fps);
    updateAnimationsList();

    // Sélectionner la nouvelle animation
    QList<QTreeWidgetItem*> items = ui->animationList->findItems(name, Qt::MatchExactly, 0);
    if (!items.isEmpty()) {
        ui->animationList->setCurrentItem(items.first());
        startAnimation();
    }

    for (int c = 0; c < ui->animationList->columnCount(); c++) {
        ui->animationList->resizeColumnToContents(c);
    }
}

void MainWindow::updateAnimationsList()
{
    if (!extractor) return;

    // Bloquer les signaux pour éviter les appels récursifs
    ui->animationList->blockSignals(true);
    ui->fps->blockSignals(true);

    // Sauvegarder l'état actuel
    QStringList previouslySelectedNames;
    QList<QTreeWidgetItem*> currentSelectedItems = ui->animationList->selectedItems();
    for (QTreeWidgetItem* item : currentSelectedItems) {
        previouslySelectedNames.append(item->text(0));
    }

    int currentFps = ui->fps->value();

    try {
        // Mettre à jour le modèle Extractor pour les animations sélectionnées
        for (const QString &animName : previouslySelectedNames) {
            if (extractor->getAnimationNames().contains(animName)) {
                QList<int> frameIndices = extractor->getAnimationFrames(animName);
                extractor->setAnimation(animName, frameIndices, currentFps);
            }
        }

        // Synchroniser l'affichage
        syncAnimationListWidget();

        // Restaurer la sélection
        for (const QString &animName : previouslySelectedNames) {
            QList<QTreeWidgetItem*> items = ui->animationList->findItems(animName, Qt::MatchExactly, 0);
            if (!items.isEmpty()) {
                items.first()->setSelected(true);
            }
        }

    } catch (const std::exception& e) {
        qWarning() << "Erreur lors de la mise à jour des animations:" << e.what();
    } catch (...) {
        qWarning() << "Erreur inconnue lors de la mise à jour des animations";
    }

    // Restaurer les signaux
    ui->animationList->blockSignals(false);
    ui->fps->blockSignals(false);
}

void MainWindow::syncAnimationListWidget()
{
    if (!extractor) return;

    // Bloquer les signaux pendant la mise à jour
    ui->animationList->blockSignals(true);

    // Sauvegarder la sélection par NOMS (pas par pointeurs)
    QStringList selectedAnimationNames;
    QList<QTreeWidgetItem*> currentSelected = ui->animationList->selectedItems();
    for (QTreeWidgetItem* item : currentSelected) {
        selectedAnimationNames.append(item->text(0));
    }

    // Effacer et reconstruire la liste
    ui->animationList->clear();

    QStringList animationNames = extractor->getAnimationNames();
    for (const QString &name : animationNames) {
        QList<int> frameIndices = extractor->getAnimationFrames(name);
        int fps = extractor->getAnimationFps(name);

        QTreeWidgetItem *item = new QTreeWidgetItem(ui->animationList);
        item->setText(0, name);
        item->setText(1, QString::number(fps));

        // Convertir les indices en string
        QStringList framesStrList;
        for (int frameIndex : frameIndices) {
            framesStrList << QString::number(frameIndex);
        }
        item->setText(2, framesStrList.join(", "));

        // Restaurer la sélection
        if (selectedAnimationNames.contains(name)) {
            item->setSelected(true);
        }
    }

    // Restaurer les signaux
    ui->animationList->blockSignals(false);
}

void MainWindow::startAnimation()
{
    stopAnimationTimer();

    if (!canStartAnimation()) {
        return;
    }

    setupAnimationParameters();
    startAnimationTimer();

    updateAnimationUI(true); // playing state
}

bool MainWindow::canStartAnimation() const
{
    QList<QTreeWidgetItem*> selectedAnimations = ui->animationList->selectedItems();
    if (selectedAnimations.isEmpty()) {
        return false;
    }

    QString animationName = selectedAnimations.first()->text(0);
    return extractor->getAnimationNames().contains(animationName);
}

void MainWindow::setupAnimationParameters()
{
    QList<QTreeWidgetItem*> selectedAnimations = ui->animationList->selectedItems();
    QString animationName = selectedAnimations.first()->text(0);

    selectedFrameRows = extractor->getAnimationFrames(animationName);

    // Filtrer les frames valides
    QList<int> validFrameRows;
    for (int frameIndex : selectedFrameRows) {
        if (frameIndex >= 0 && frameIndex < extractor->m_frames.size()) {
            validFrameRows.append(frameIndex);
        }
    }
    selectedFrameRows = validFrameRows;

    // Mettre à jour le FPS
    int animationFps = extractor->getAnimationFps(animationName);
    if (animationFps > 0) {
        ui->fps->blockSignals(true);
        ui->fps->setValue(animationFps);
        ui->fps->blockSignals(false);
    }

    // Configurer l'UI
    setupAnimationUI();
}

void MainWindow::setupAnimationUI()
{
    int frameCount = selectedFrameRows.size();
    int fps = ui->fps->value();
    if (fps <= 0) fps = 1;

    double msPerFrame = 1000.0 / (double)fps;
    int totalDurationMs = (int)(frameCount * msPerFrame);

    QTime durationTime(0, 0, 0);
    durationTime = durationTime.addMSecs(totalDurationMs);

    ui->timeTo->setDisplayFormat("mm:ss:zzz");
    ui->timeTo->setTime(durationTime);

    ui->sliderFrom->setMaximum(qMax(0, frameCount - 1));
}

void MainWindow::startAnimationTimer()
{
    int fpsValue = ui->fps->value();
    int intervalMs = (fpsValue > 0) ? (1000 / fpsValue) : 100;

    currentAnimationFrameIndex = 0;
    animationTimer->start(intervalMs);
}

void MainWindow::stopAnimationTimer()
{
    if (animationTimer->isActive()) {
        animationTimer->stop();
    }
}

void MainWindow::updateAnimationUI(bool playing)
{
    ui->Play->setVisible(!playing);
    ui->Pause->setVisible(playing);
}


void MainWindow::stopAnimation()
{
    animationTimer->stop();
    currentAnimationFrameIndex = 0;

    // Réinitialiser l'interface
    ui->sliderFrom->blockSignals(true);
    ui->sliderFrom->setValue(0);
    ui->sliderFrom->blockSignals(false);

    QTime zeroTime(0, 0, 0);
    ui->timeFrom->setTime(zeroTime);

    ui->Play->setVisible(true);
    ui->Pause->setVisible(false);
}

void MainWindow::updateAnimation()
{
    // Vérifications de sécurité renforcées
    if (!extractor || extractor->m_frames.isEmpty()) {
        stopAnimation();
        return;
    }

    if (selectedFrameRows.isEmpty()) {
        stopAnimation();
        return;
    }

    if (currentAnimationFrameIndex < 0 || currentAnimationFrameIndex >= selectedFrameRows.size()) {
        currentAnimationFrameIndex = 0;
    }

    int frameListIndex = selectedFrameRows.at(currentAnimationFrameIndex);

    // Validation renforcée de l'index
    if (frameListIndex < 0 || frameListIndex >= extractor->m_frames.size()) {
        qWarning() << "Invalid frame index in animation:" << frameListIndex;
        stopAnimation();
        return;
    }

    // Le reste du code reste identique...
    ui->sliderFrom->blockSignals(true);
    ui->sliderFrom->setValue(currentAnimationFrameIndex);
    ui->sliderFrom->blockSignals(false);

    int fps = ui->fps->value();
    if (fps <= 0) fps = 60;

    double msPerFrame = 1000.0 / (double)fps;
    int currentMs = (int)(currentAnimationFrameIndex * msPerFrame);

    QTime currentTime(0, 0, 0);
    currentTime = currentTime.addMSecs(currentMs);

    ui->timeFrom->setDisplayFormat("mm:ss:zzz");
    ui->timeFrom->setTime(currentTime);

    const QPixmap &currentFrame = extractor->m_frames.at(frameListIndex);

    QGraphicsScene *scene = ui->graphicsViewResult->scene();
    scene->clear();

    scene->setSceneRect(0, 0, extractor->m_maxFrameWidth, extractor->m_maxFrameHeight);

    QGraphicsPixmapItem *item = scene->addPixmap(currentFrame);

    qreal x_offset = (extractor->m_maxFrameWidth - currentFrame.width()) / 2.0;
    qreal y_offset = (extractor->m_maxFrameHeight - currentFrame.height()) / 2.0;
    item->setPos(x_offset, y_offset);

    currentAnimationFrameIndex++;
    if (currentAnimationFrameIndex >= selectedFrameRows.size()) {
        currentAnimationFrameIndex = 0;
    }
}

void MainWindow::updateCurrentAnimation()
{
    QList<int> selectedIndices = getSelectedFrameIndices();

    if (selectedIndices.isEmpty()) {
        // Pas de sélection, supprimer l'animation "current"
        stopAnimation();
        removeCurrentAnimation();
    } else {
        // Créer ou mettre à jour l'animation "current"
        createAnimation("current", selectedIndices, ui->fps->value());

        ui->animationList->setCurrentItem(ui->animationList->findItems("current", Qt::MatchExactly).constFirst());
        startAnimation();
    }
}

void MainWindow::removeCurrentAnimation()
{
    if (hasCurrentAnimation()) {
        extractor->removeAnimation("current");
        syncAnimationListWidget();

        // Si l'animation "current" était sélectionnée, arrêter l'animation
        QList<QTreeWidgetItem*> selectedItems = ui->animationList->selectedItems();
        if (!selectedItems.isEmpty() && selectedItems.first()->text(0) == "current") {
            stopAnimation();
        }
    }
}

bool MainWindow::hasCurrentAnimation() const
{
    if (!extractor) return false;
    return extractor->getAnimationNames().contains("current");
}
