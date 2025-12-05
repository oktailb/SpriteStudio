#include "include/mainwindow.h"
#include "./ui_mainwindow.h"
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

void MainWindow::createAnimationFromSelection()
{
    QList<int> selectedIndices;
    if (currentSelection.isEmpty())
        selectedIndices = getSelectedFrameIndices();
    else
        selectedIndices = currentSelection;

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
    if (name == "current") {
        extractor->setAnimation(name, currentSelection, fps);

        updateAnimationsList();

        for (int c = 0; c < ui->animationList->columnCount(); c++) {
            ui->animationList->resizeColumnToContents(c);
        }
        return;
    }

    extractor->setAnimation(name, selectedIndices, fps);
    updateAnimationsList();

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

    ui->animationList->blockSignals(true);
    ui->fps->blockSignals(true);

    QStringList previouslySelectedNames;
    QList<QTreeWidgetItem*> currentSelectedItems = ui->animationList->selectedItems();
    for (QTreeWidgetItem* item : currentSelectedItems) {
        previouslySelectedNames.append(item->text(0));
    }

    int currentFps = ui->fps->value();

    try {
        for (const QString &animName : previouslySelectedNames) {
            if (extractor->getAnimationNames().contains(animName)) {
                QList<int> frameIndices = extractor->getAnimationFrames(animName);
                extractor->setAnimation(animName, frameIndices, currentFps);
            }
        }

        syncAnimationListWidget();

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

    ui->animationList->blockSignals(false);
    ui->fps->blockSignals(false);
}

void MainWindow::reverseAnimationOrder()
{
    QList<QTreeWidgetItem*> selectedAnimations = ui->animationList->selectedItems();
    if (selectedAnimations.isEmpty()) {
        QMessageBox::information(this, tr("_info"), tr("_select_animation_first"));
        return;
    }

    QTreeWidgetItem* selectedAnimation = selectedAnimations.first();
    QString animationName = selectedAnimation->text(0);

    extractor->reverseAnimationFrames(animationName);
    syncAnimationListWidget();

    if (animationTimer->isActive()) {
        startAnimation();
    }
}

void MainWindow::syncAnimationListWidget()
{
  if (!extractor) return;

  ui->animationList->blockSignals(true);

         // Store current selection to restore it after refresh
  QString currentSelectedName;
  QList<QTreeWidgetItem*> selectedItems = ui->animationList->selectedItems();
  if (!selectedItems.isEmpty()) {
      currentSelectedName = selectedItems.first()->text(0);
    }

  ui->animationList->clear();

  QStringList animationNames = extractor->getAnimationNames();
  for (const QString &name : animationNames) {
      QList<int> frameIndices = extractor->getAnimationFrames(name);
      int fps = extractor->getAnimationFps(name);

      QTreeWidgetItem *item = new QTreeWidgetItem(ui->animationList);
      item->setText(0, name);
      item->setText(1, QString::number(fps));

             // Show user numbers (1-index) in the order stored in the animation
      QStringList framesStrList;
      for (int frameIndex : frameIndices) {
          framesStrList << QString::number(frameIndex + 1);
        }
      item->setText(2, framesStrList.join(", "));

             // Restore selection
      if (name == currentSelectedName) {
          item->setSelected(true);
        }
    }

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

    QList<int> validFrameRows;
    for (int frameIndex : selectedFrameRows) {
        if (frameIndex >= 0 && frameIndex < extractor->m_frames.size()) {
            validFrameRows.append(frameIndex);
        }
    }
    selectedFrameRows = validFrameRows;

    int animationFps = extractor->getAnimationFps(animationName);
    if (animationFps > 0) {
        ui->fps->blockSignals(true);
        ui->fps->setValue(animationFps);
        ui->fps->blockSignals(false);
    }

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

    if (frameListIndex < 0 || frameListIndex >= extractor->m_frames.size()) {
        qWarning() << "Invalid frame index in animation:" << frameListIndex;
        stopAnimation();
        return;
    }

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
  // Always use currentSelection for real-time updates during selection
  if (currentSelection.isEmpty()) {
      removeCurrentAnimation();
    } else {
      // Update the "current" animation with the current selection order
      extractor->setAnimation("current", currentSelection, ui->fps->value());

      // Force sync of the animation list widget
      syncAnimationListWidget();

      // Select the "current" animation in the list
      QList<QTreeWidgetItem*> currentItems = ui->animationList->findItems("current", Qt::MatchExactly);
      if (!currentItems.isEmpty()) {
          ui->animationList->setCurrentItem(currentItems.first());
        }
    }
}

void MainWindow::removeCurrentAnimation()
{
  if (hasCurrentAnimation()) {
      extractor->removeAnimation("current");
      syncAnimationListWidget();

      // If current was selected, cleanup and stop animation
      QList<QTreeWidgetItem*> selectedItems = ui->animationList->selectedItems();
      if (!selectedItems.isEmpty() && selectedItems.first()->text(0) == "current") {
          selectedItems.removeFirst();
        }
      clearFrameSelections();
      stopAnimation();
      on_Pause_clicked();
    }
}

bool MainWindow::hasCurrentAnimation() const
{
    if (!extractor) return false;
    return extractor->getAnimationNames().contains("current");
}
