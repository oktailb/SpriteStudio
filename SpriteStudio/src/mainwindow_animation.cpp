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
    QModelIndexList selectedIndexes = ui->framesList->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;

    std::sort(selectedIndexes.begin(), selectedIndexes.end(),
              [](const QModelIndex& a, const QModelIndex& b) {
                  return a.row() < b.row();
              });

    bool ok;
    QString text = QInputDialog::getText(this, tr("_new_animation"),
                                         tr("_animation_name"), QLineEdit::Normal,
                                         tr("_new_animation"), &ok);

    if (!ok || text.isEmpty()) return;

    int currentFps = ui->fps->value();

    QStringList framesStrList;
    for (const QModelIndex &idx : selectedIndexes) {
        framesStrList << QString::number(idx.row());
    }
    QString framesString = framesStrList.join(", ");

    if (ui->animationList) {
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->animationList);
        item->setText(0, text);
        item->setText(1, QString::number(currentFps));
        item->setText(2, framesString);

        ui->animationList->setCurrentItem(item);
    }
}

void MainWindow::startAnimation()
{
  if (animationTimer->isActive()) {
      animationTimer->stop();
    }

  selectedFrameRows.clear();
  QModelIndexList selected = ui->framesList->selectionModel()->selectedIndexes();

  for (const QModelIndex &index : selected) {
      selectedFrameRows.append(index.row());
    }

  int frameCount = selectedFrameRows.size();
  int fps = ui->fps->value();
  if (fps <= 0) fps = 1;

  double msPerFrame = 1000.0 / (double)fps;
  int totalDurationMs = (int)(frameCount * msPerFrame);

  QTime durationTime(0, 0, 0);
  durationTime = durationTime.addMSecs(totalDurationMs);

  ui->timeTo->setDisplayFormat("mm:ss:zzz");
  ui->timeTo->setTime(durationTime);

  if (frameCount > 0) {
      ui->sliderFrom->setMaximum(frameCount - 1);
    } else {
      ui->sliderFrom->setMaximum(0);
    }

  if (selectedFrameRows.isEmpty()) {
      stopAnimation();
      return;
    }

  int fpsValue = ui->fps->value();
  int intervalMs = (fpsValue > 0) ? (1000 / fpsValue) : 100;

  currentAnimationFrameIndex = 0;

  animationTimer->start(intervalMs);
}

void MainWindow::stopAnimation()
{
  animationTimer->stop();
  currentAnimationFrameIndex = 0;
}

void MainWindow::updateAnimation()
{
  if (selectedFrameRows.isEmpty() || extractor->m_frames.isEmpty()) {
      stopAnimation();
      return;
    }

  int frameListIndex = selectedFrameRows.at(currentAnimationFrameIndex);

  if (frameListIndex < 0 || frameListIndex >= extractor->m_frames.size()) {
      qWarning() << tr("_error") << "Frame index invalid.";
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

  QGraphicsPixmapItem *item =scene->addPixmap(currentFrame);

  // Center the animation
  qreal x_offset = (extractor->m_maxFrameWidth - currentFrame.width()) / 2.0;
  qreal y_offset = (extractor->m_maxFrameHeight - currentFrame.height()) / 2.0;
  item->setPos(x_offset, y_offset);

  currentAnimationFrameIndex++;
  if (currentAnimationFrameIndex >= selectedFrameRows.size()) {
      currentAnimationFrameIndex = 0; // Forever loop
    }
}
