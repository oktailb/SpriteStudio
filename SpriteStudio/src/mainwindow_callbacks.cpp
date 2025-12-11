#include "mainwindow.h"
#include "aboutdialog.h"
#include "./ui_mainwindow.h"
#include "qfiledialog.h"
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
#include "extractor/gifextractor.h"
#include "extractor/spriteextractor.h"
#include "extractor/jsonextractor.h"

void MainWindow::onAtlasContextMenuRequested(const QPoint &pos)
{
    if (!extractor || extractor->m_atlas.isNull())
        return;

    QMenu menu(this);
    QList<int> selectedIndices = getSelectedFrameIndices();

    if (!selectedIndices.isEmpty()) {
        if (selectedIndices.size() > 1) {
            QAction *createAnimAction = menu.addAction(tr("_create_animation"));
            connect(createAnimAction, &QAction::triggered,
                    this, &MainWindow::createAnimationFromSelection);

            menu.addSeparator();
        }

        QString deleteText = (selectedIndices.size() > 1) ?
                                 tr("_delete_selected_frames") : tr("_delete_frame");
        QAction *deleteAction = menu.addAction(deleteText);
        connect(deleteAction, &QAction::triggered,
                this, &MainWindow::deleteSelectedFramesFromAtlas);

        menu.addSeparator();

        QAction *invertAction = menu.addAction(tr("_invert_selection"));
        connect(invertAction, &QAction::triggered,
                this, &MainWindow::invertSelection);
    }
    else {
        QAction *removeBgAction = menu.addAction(tr("_delete_background"));
        connect(removeBgAction, &QAction::triggered, this, &MainWindow::removeAtlasBackgroundAndRefresh);
    }
    menu.exec(ui->graphicsViewLayers->mapToGlobal(pos));
}

void MainWindow::zoomSliderChanged(int val)
{
    zoomLabel->setText(QString::number(val) + "%");
    zoomFactor = val / 100.0;
    ui->graphicsViewLayers->resetTransform();
    ui->graphicsViewLayers->scale(zoomFactor, zoomFactor);
}

void MainWindow::on_animationList_itemSelectionChanged()
{
    QList<QTreeWidgetItem*> selectedItems = ui->animationList->selectedItems();

    if (selectedItems.isEmpty()) {
        stopAnimation();
        return;
    }

    QTreeWidgetItem *item = selectedItems.first();
    QString animationName = item->text(0);

    QList<int> frameIndices = extractor->getAnimationFrames(animationName);
    int savedFps = extractor->getAnimationFps(animationName);

    if (savedFps > 0) {
        ui->fps->blockSignals(true);
        ui->fps->setValue(savedFps);
        ui->fps->blockSignals(false);
    }

    QTimer::singleShot(0, this, [this]() {
        startAnimation();
    });
}

void MainWindow::on_animationList_itemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    if (ui->animationList->selectedItems().contains(item) && animationTimer->isActive()) {
        ui->animationList->clearSelection();
    }
    ui->animationList->setCurrentItem(item);

    clearBoundingBoxHighlighters();
    setBoundingBoxHighllithers(selectedFrameRows);
    adjustZoomSliderToWindow();
}

void MainWindow::removeSelectedAnimation()
{
    QList<QTreeWidgetItem*> selectedItems = ui->animationList->selectedItems();
    if (selectedItems.isEmpty()) return;

    QStringList animationNames;
    for (QTreeWidgetItem* item : selectedItems) {
        QString name = item->text(0);
        if (name != "current") {
            animationNames.append(name);
        }
    }

    if (!animationNames.isEmpty()) {
        removeAnimations(animationNames);
    }
}

void MainWindow::removeAnimations(const QStringList &animationNames)
{
    if (animationNames.isEmpty()) return;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("_confirm"),
                                  tr("_confirm_delete", "", animationNames.size()),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        for (const QString &animationName : animationNames) {
            extractor->removeAnimation(animationName);
        }
        syncAnimationListWidget();
        stopAnimation();
    }
}

void MainWindow::on_animationList_customContextMenuRequested(const QPoint &pos)
{
    QTreeWidgetItem *item = ui->animationList->itemAt(pos);

    if (item) {
        QMenu menu(this);

        QAction *deleteAction = menu.addAction(tr("_delete_animation"));
        connect(deleteAction, &QAction::triggered,
                this, &MainWindow::removeSelectedAnimation);

        QAction *reverseOrderAction = menu.addAction(tr("_reverse_order"));
        connect(reverseOrderAction, &QAction::triggered,
                this, &MainWindow::reverseAnimationOrder);

        menu.exec(ui->animationList->viewport()->mapToGlobal(pos));
    }
}

void MainWindow::on_framesList_customContextMenuRequested(const QPoint &pos)
{

}

void MainWindow::updateAnimationsAfterFrameRemoval(int removedRow, int mergedRow)
{
    if (!extractor) return;

    QStringList animationNames = extractor->getAnimationNames();
    for (const QString &name : animationNames) {
        QList<int> frameIndices = extractor->getAnimationFrames(name);
        QList<int> updatedIndices;
        int fps = extractor->getAnimationFps(name);

        for (int frameIndex : frameIndices) {
            if (frameIndex == removedRow) {
                if (!updatedIndices.contains(mergedRow)) {
                    updatedIndices.append(mergedRow);
                }
            } else if (frameIndex > removedRow) {
                updatedIndices.append(frameIndex - 1);
            } else {
                updatedIndices.append(frameIndex);
            }
        }

        QSet<int> uniqueIndices;
        for (int index : updatedIndices) {
            uniqueIndices.insert(index);
        }
        updatedIndices = uniqueIndices.values();

        std::sort(updatedIndices.begin(), updatedIndices.end());
        extractor->setAnimation(name, updatedIndices, fps);
    }
}

void MainWindow::onMergeFrames(int sourceRow, int targetRow)
{
  if (sourceRow < 0 || sourceRow >= extractor->m_atlas_index.size() ||
      targetRow < 0 || targetRow >= extractor->m_atlas_index.size()) {
      return;
    }

    QString currentAnimationName;
    QList<QTreeWidgetItem*> selectedAnimations = ui->animationList->selectedItems();
    if (!selectedAnimations.isEmpty()) {
        currentAnimationName = selectedAnimations.first()->text(0);
    }

    // 1. Compute and merge
    Extractor::Box srcBox = extractor->m_atlas_index[sourceRow];
    Extractor::Box tgtBox = extractor->m_atlas_index[targetRow];

    QRect srcRect(srcBox.rect);
    QRect tgtRect(tgtBox.rect);
    QRect unitedRect = srcRect.united(tgtRect);

    QPixmap mergedPixmap = QPixmap::fromImage(this->extractor->m_atlas).copy(unitedRect);

    // 2. Update target internal data
    Extractor::Box newBox;
    newBox.rect = unitedRect;
    newBox.selected = srcBox.selected || tgtBox.selected; // Conserver la sélection
    newBox.index = tgtBox.index; // Conserver l'index original

    extractor->m_atlas_index[targetRow] = newBox;
    extractor->m_frames[targetRow] = mergedPixmap;

    // 3. update the item in the extractor data model
    if (targetRow < frameModel->rowCount()) {
        QStandardItem *targetItem = frameModel->item(targetRow, 0);
        if (targetItem) {
            QPixmap thumbnail = mergedPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            targetItem->setData(thumbnail, Qt::DecorationRole);
            // Utiliser +1 pour l'affichage utilisateur
            targetItem->setData(QString("Frame %1 + %2 merged")
                                     .arg(sourceRow + 1).arg(targetRow + 1),
                                 Qt::DisplayRole);
          }
      }
    extractor->removeFrame(sourceRow);
    populateFrameList(extractor->m_frames, extractor->m_atlas_index);
    syncAnimationListWidget();
    if (!currentAnimationName.isEmpty()) {
        QList<QTreeWidgetItem*> items = ui->animationList->findItems(currentAnimationName, Qt::MatchExactly, 0);
        if (!items.isEmpty()) {
            ui->animationList->setCurrentItem(items.first());
        } else {
            stopAnimation();
        }
    } else {
        stopAnimation();
    }

    // Graphical cleanup
    clearBoundingBoxHighlighters();
}

QString readTextFile(const QString &filePath)
{
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return QString("Error: Cannot open file %1").arg(filePath);
    }
  QTextStream in(&file);
  return in.readAll();
}

void MainWindow::on_actionLicence_triggered()
{
  QString licenseText = readTextFile(":/text/license.txt");
  QDialog dialog;
  dialog.setWindowTitle("Licence");
  dialog.setMinimumSize(600, 400);
  QTextEdit *textEdit = new QTextEdit(&dialog);
  textEdit->setPlainText(licenseText);
  textEdit->setReadOnly(true);
  QPushButton *closeButton = new QPushButton("Close", &dialog);
  QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->addWidget(textEdit);
  layout->addWidget(closeButton);
  dialog.exec();
}

void MainWindow::on_actionAbout_triggered()
{
  AboutDialog aboutDialog(this);
  aboutDialog.exec();
}

void MainWindow::on_actionOpen_triggered()
{
  const QString title = tr("_open_file");
  const QString formats = tr("_images") + " (*.png *.jpg *.jpeg *.bmp *.gif *.json)";
  QString fileName = QFileDialog::getOpenFileName(this, title, "", formats);
  currentFilePath = fileName;
  processFile(currentFilePath);
  statusLabel->setText(fileName);
  setWindowTitle("SpriteStudio (" + fileName + ")");
  ui->verticalTolerance->setValue(extractor->m_maxFrameHeight / 3);
  adjustZoomSliderToWindow();
}

void MainWindow::on_actionExit_triggered()
{
  delete extractor;
  exit(EXIT_SUCCESS);
}

void MainWindow::on_fps_valueChanged(int fps)
{
    ui->timingLabel->setText(" -> " + tr("_timing") + ": " + QString::number(1000.0  / (double)fps, 'g', 4) + "ms");

    updateAnimationsList();
}

void MainWindow::on_alphaThreshold_valueChanged(int threshold)
{
  Q_UNUSED(threshold);
  if (!currentFilePath.isEmpty()) {
      processFile(currentFilePath);
    }
}

void MainWindow::on_enableSmartCropCheckbox_stateChanged(int state)
{
    if (extractor == nullptr) return;
    if (!currentFilePath.isEmpty()) {
        extractor->setSmartCropEnabled(state != 0);
        connect(extractor, &Extractor::extractionFinished,
                this, [this]() {

                    this->populateFrameList(extractor->m_frames, extractor->m_atlas_index);
                    // Ensure animation is started after the loading
                    this->setupGraphicsView(extractor->m_atlas);
                    this->stopAnimation();
                    this->startAnimation();
                    ui->verticalTolerance->setValue(extractor->m_maxFrameHeight / 3);
                });

        if  (ui->backgroundRemoval->isChecked())
            removeAtlasBackground();;
        extractor->extractFromPixmap(ui->alphaThreshold->value(), ui->verticalTolerance->value());
        clearBoundingBoxHighlighters();
        if (!extractor->m_frames.isEmpty()) {
            auto view = ui->graphicsViewLayers;
            auto scene = new QGraphicsScene(view);

            QGraphicsPixmapItem *item = new QGraphicsPixmapItem(QPixmap::fromImage(extractor->m_atlas));
            scene->addItem(item);
            item->setPos(0, 0);
            view->setScene(scene);
            view->show();

            populateFrameList(extractor->m_frames, extractor->m_atlas_index);
        }

    }
}

void MainWindow::on_overlapThresholdSpinbox_valueChanged(double threshold)
{
    if (extractor == nullptr)
        return;
    if (!currentFilePath.isEmpty()) {
        extractor->setOverlapThreshold(threshold);
        connect(extractor, &Extractor::extractionFinished,
                this, [this]() {

                    this->populateFrameList(extractor->m_frames, extractor->m_atlas_index);
                    // Ensure animation is started after the loading
                    this->setupGraphicsView(extractor->m_atlas);
                    this->stopAnimation();
                    this->startAnimation();
                    ui->verticalTolerance->setValue(extractor->m_maxFrameHeight / 3);
                });

        if  (ui->backgroundRemoval->isChecked())
            removeAtlasBackground();;
        extractor->extractFromPixmap(ui->alphaThreshold->value(), ui->verticalTolerance->value());
        clearBoundingBoxHighlighters();
        if (!extractor->m_frames.isEmpty()) {
            auto view = ui->graphicsViewLayers;
            auto scene = new QGraphicsScene(view);

            QGraphicsPixmapItem *item = new QGraphicsPixmapItem(QPixmap::fromImage(extractor->m_atlas));
            scene->addItem(item);
            item->setPos(0, 0);
            view->setScene(scene);
            view->show();

            populateFrameList(extractor->m_frames, extractor->m_atlas_index);
        }
    }
}

void MainWindow::on_verticalTolerance_valueChanged(int verticalTolerance)
{
  Q_UNUSED(verticalTolerance);
  if (!extractor) {
      if (!currentFilePath.isEmpty()) {
          processFile(currentFilePath);
        }
    }
  else
    {
      extractor->extractFromPixmap(ui->alphaThreshold->value(), ui->verticalTolerance->value());
    }
}

QColor MainWindow::getHighlightColor(int index, int total)
{
  if (total == 1) return Qt::magenta; // Single selection => magenta

  // Multiple selection => rainbow
  static QList<QColor> colorPalette = {
    QColor(255, 0, 0, 50),
    QColor(0, 150, 0, 50),
    QColor(0, 0, 255, 50),
    QColor(255, 165, 0, 50),
    QColor(128, 0, 128, 50),
    QColor(0, 128, 128, 50),
    QColor(165, 42, 42, 50),
    QColor(255, 192, 203, 50)
  };

  return colorPalette.at(index % colorPalette.size());
}

void MainWindow::on_framesList_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);

    if (!ui->graphicsViewLayers->scene() || !extractor) {
        return;
    }
    statusLabel->setText("Frame " + QString::number(1 + index.data(Qt::UserRole).toInt()));

    QList<int> selectedIndices;
    selectedIndices.push_back(index.data(Qt::UserRole).toInt());

    clearBoundingBoxHighlighters();
    setBoundingBoxHighllithers(selectedIndices);
    adjustZoomSliderToWindow();
}

void MainWindow::on_actionExport_triggered()
{
  // Check if have frames
  if (extractor->m_frames.isEmpty()) {
      QMessageBox::warning(this, tr("_export_error"), tr("_please_load_frames"));
      return;
    }
  Extractor * extractorOut = nullptr;
  const QString filter = tr("_export_formats_json") + "(*.png *.json);;" + tr("_export_formats_png") + "(*.png)";

  QString selectedFile = QFileDialog::getSaveFileName(
      this,
      tr("_export_atlas"),
      QDir::homePath(), // Default path TODO: remember previous if export is used multiple times
      filter
      );

  if (selectedFile.isEmpty()) {
      return; // Cancel case
    }

  QFileInfo fileInfo(selectedFile);
  QString filePath = fileInfo.absolutePath();
  QString baseName = fileInfo.completeBaseName();
  QString extension = fileInfo.suffix().toLower();

  try {
      if (extension == "json") {
          extractorOut = new JsonExtractor(statusLabel, progressBar, this);
        } else if (extension == "png") {
          extractorOut = new SpriteExtractor(statusLabel, progressBar, this);
        } else if (extension == "gif") {
          extractorOut = new GifExtractor(statusLabel, progressBar, this);
        } else {
          extractorOut = new JsonExtractor(statusLabel, progressBar, this);
        }

        bool success = extractorOut->exportFrames(filePath, baseName, extractor);
      if (!success) {
          QMessageBox::warning(this, tr("Export Error"), tr("Export failed"));
        }
    } catch (...) {
      QMessageBox::critical(this, tr("Export Error"), tr("Export crashed"));
    }
  delete extractorOut;
}

void MainWindow::on_Play_clicked()
{
    QList<QTreeWidgetItem*> selectedAnimations = ui->animationList->selectedItems();
    if (selectedAnimations.isEmpty()) {
        return;
    }

    startAnimation();
}

void MainWindow::on_Pause_clicked()
{
    if (animationTimer->isActive()) {
        animationTimer->stop();
    }

    ui->Play->setVisible(true);
    ui->Pause->setVisible(false);
}

void MainWindow::on_spriteCleanButton_clicked()
{

}

void MainWindow::on_spriteAlignButton_clicked()
{
    QGraphicsScene *scene = ui->graphicsViewResult->scene();
    QList<QTreeWidgetItem*> selectedAnimations = ui->animationList->selectedItems();
    QString animationName = selectedAnimations.first()->text(0);

    scene->clear();
    scene->setSceneRect(0, 0, extractor->m_maxFrameWidth, extractor->m_maxFrameHeight);

    for (int frameId : extractor->m_animationsData[animationName].frameIndices) {
        QPixmap currentFrame = extractor->m_frames[frameId];
        QGraphicsPixmapItem *item = scene->addPixmap(currentFrame);

        qreal x_offset = (extractor->m_maxFrameWidth - currentFrame.width()) / 2.0;
        qreal y_offset = (extractor->m_maxFrameHeight - currentFrame.height()) / 2.0;
        item->setPos(x_offset, y_offset);
    }
}

void MainWindow::on_mirrorButton_clicked()
{
    QList<QTreeWidgetItem*> selectedAnimations = ui->animationList->selectedItems();
    QString animationName = selectedAnimations.first()->text(0);
    for (int frameId : extractor->m_animationsData[animationName].frameIndices) {
        extractor->m_frames[frameId] = extractor->m_frames[frameId].transformed(QTransform().scale(-1, 1));
    }
    updateAnimation();
}

void MainWindow::on_sliderFrom_sliderMoved(int position)
{
    currentAnimationFrameIndex = position;
    on_Pause_clicked();
    updateAnimation();
}
