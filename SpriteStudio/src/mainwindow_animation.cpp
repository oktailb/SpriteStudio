#include "include/mainwindow.h"
#include "ui_mainwindow.h"
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
        m_document->setAnimation(name, currentSelection, fps, true);
        syncFromDocument();
        return;
    }

    m_undoStack->push(new CreateAnimationCommand(m_document, name, selectedIndices, fps));

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
    if (!m_document) return;

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
            if (m_document->hasAnimation(animName)) {
                SpriteAnimation anim = m_document->animation(animName);
                m_document->setAnimation(animName, anim.frameIndices, currentFps, anim.loop);
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

    m_undoStack->push(new ReverseAnimationCommand(m_document, animationName));
}

void MainWindow::syncAnimationListWidget()
{
    if (!m_document) return;

    ui->animationList->blockSignals(true);

    // Store current selection to restore it after refresh
    QString currentSelectedName;
    QList<QTreeWidgetItem*> selectedItems = ui->animationList->selectedItems();
    if (!selectedItems.isEmpty()) {
        currentSelectedName = selectedItems.first()->text(0);
    }

    ui->animationList->clear();

    const auto &animMap = m_document->animations();
    for (auto it = animMap.begin(); it != animMap.end(); ++it) {
        const QString &name = it.key();
        const SpriteAnimation &anim = it.value();

        QTreeWidgetItem *item = new QTreeWidgetItem(ui->animationList);
        item->setText(0, name);
        item->setText(1, QString::number(anim.fps));

        // Show user numbers (1-index) in the order stored in the animation
        QStringList framesStrList;
        for (int frameIndex : anim.frameIndices) {
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
    if (!m_document) return false;
    QList<QTreeWidgetItem*> selectedAnimations = ui->animationList->selectedItems();
    if (selectedAnimations.isEmpty()) {
        return false;
    }

    QString animationName = selectedAnimations.first()->text(0);
    return m_document->hasAnimation(animationName);
}

void MainWindow::setupAnimationParameters()
{
    if (!m_document) return;
    QList<QTreeWidgetItem*> selectedAnimations = ui->animationList->selectedItems();
    if (selectedAnimations.isEmpty()) return;

    QString animationName = selectedAnimations.first()->text(0);
    SpriteAnimation anim = m_document->animation(animationName);

    selectedFrameRows = anim.frameIndices;

    QList<int> validFrameRows;
    for (int frameIndex : selectedFrameRows) {
        if (frameIndex >= 0 && frameIndex < m_document->frameCount()) {
            validFrameRows.append(frameIndex);
        }
    }
    selectedFrameRows = validFrameRows;

    if (anim.fps > 0) {
        ui->fps->blockSignals(true);
        ui->fps->setValue(anim.fps);
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
    if (!m_document || m_document->frameCount() == 0) {
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

    if (frameListIndex < 0 || frameListIndex >= m_document->frameCount()) {
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

    const QPixmap &currentFrame = m_document->frame(frameListIndex);

    QGraphicsScene *scene = ui->graphicsViewResult->scene();
    scene->setSceneRect(0, 0, m_document->maxFrameWidth(), m_document->maxFrameHeight());

    QGraphicsPixmapItem *item = nullptr;
    const auto items = scene->items();
    for (auto *i : items) {
        item = dynamic_cast<QGraphicsPixmapItem*>(i);
        if (item) break;
    }

    if (!item) {
        scene->clear();
        item = scene->addPixmap(currentFrame);
    } else {
        item->setPixmap(currentFrame);
    }

    qreal x_offset = (m_document->maxFrameWidth() - currentFrame.width()) / 2.0;
    qreal y_offset = (m_document->maxFrameHeight() - currentFrame.height()) / 2.0;
    item->setPos(x_offset, y_offset);

    currentAnimationFrameIndex++;
    if (currentAnimationFrameIndex >= selectedFrameRows.size()) {
        currentAnimationFrameIndex = 0;
    }
}

void MainWindow::updateCurrentAnimation()
{
  if (!m_document) return;
  // Always use currentSelection for real-time updates during selection
  if (currentSelection.isEmpty()) {
      removeCurrentAnimation();
  } else {
      // Update the "current" animation with the current selection order
      m_document->setAnimation("current", currentSelection, ui->fps->value(), true);

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
      m_document->removeAnimation("current");
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
    if (!m_document) return false;
    return m_document->hasAnimation("current");
}
