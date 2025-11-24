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
#include "gifextractor.h"
#include "spriteextractor.h"
#include "jsonextractor.h"

void MainWindow::onAtlasContextMenuRequested(const QPoint &pos)
{
  // Cjeck if a frame is loaded
  if (!extractor || extractor->m_atlas.isNull())
    return;

  QMenu menu(this);
  QAction *removeBgAction = menu.addAction(tr("_delete_background"));

  // Connect the action to the treatment method
  connect(removeBgAction, &QAction::triggered, this, &MainWindow::removeAtlasBackground);

  menu.exec(ui->graphicsViewLayers->mapToGlobal(pos));
}

void MainWindow::on_framesList_customContextMenuRequested(const QPoint &pos)
{
  QModelIndexList selected = ui->framesList->selectionModel()->selectedIndexes();

  if (!selected.isEmpty()) {
      QMenu menu(this);
      QString actionText = (selected.size() > 1) ? tr("_delete_selected_frames") : tr("_delete_frame");
      QAction *deleteAction = menu.addAction(actionText);

      QObject::connect(deleteAction, &QAction::triggered,
                        this, &MainWindow::deleteSelectedFrame);

      menu.addSeparator();

      QAction *invertAction = menu.addAction(tr("_invert_selection"));
      QObject::connect(invertAction, &QAction::triggered,
                        this, &MainWindow::invertSelection);

      QAction *reverseOrderAction = menu.addAction(tr("_reverse_order"));
      QObject::connect(reverseOrderAction, &QAction::triggered,
                        this, &MainWindow::reverseSelectedFramesOrder);

      menu.exec(ui->framesList->viewport()->mapToGlobal(pos));
    }
}

void MainWindow::onMergeFrames(int sourceRow, int targetRow)
{
  if (sourceRow < 0 || sourceRow >= extractor->m_atlas_index.size() ||
      targetRow < 0 || targetRow >= extractor->m_atlas_index.size()) {
      return;
    }

  // 1. Compute and merge
  Extractor::Box srcBox = extractor->m_atlas_index[sourceRow];
  Extractor::Box tgtBox = extractor->m_atlas_index[targetRow];

  QRect srcRect(srcBox.x, srcBox.y, srcBox.w, srcBox.h);
  QRect tgtRect(tgtBox.x, tgtBox.y, tgtBox.w, tgtBox.h);
  QRect unitedRect = srcRect.united(tgtRect);

  QPixmap mergedPixmap = this->extractor->m_atlas.copy(unitedRect);

  // 2. Update target internal data
  Extractor::Box newBox;
  newBox.x = unitedRect.x();
  newBox.y = unitedRect.y();
  newBox.w = unitedRect.width();
  newBox.h = unitedRect.height();

  extractor->m_atlas_index[targetRow] = newBox;
  extractor->m_frames[targetRow] = mergedPixmap;

  // 3. Update target visual
  QStandardItem *targetItem = frameModel->item(targetRow);
  if (targetItem) {
      QPixmap thumbnail = mergedPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      targetItem->setData(thumbnail, Qt::DecorationRole);
      // Make the merge operation visible on the list
      targetItem->setData(QString("%1 + %2 merge\n[%3,%4](%5x%6)")
                               .arg(sourceRow).arg(targetRow).arg(newBox.x).arg(newBox.y).arg(newBox.w).arg(newBox.h),
                           Qt::DisplayRole);
    }

  // 4. SOURCE DELETION
  extractor->m_atlas_index.removeAt(sourceRow);
  extractor->m_frames.removeAt(sourceRow);

  // Graphical cleanup
  clearBoundingBoxHighlighters();
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
  QString aboutText = readTextFile(":/text/about.txt");
  QDialog dialog;
  dialog.setWindowTitle("About");
  dialog.setMinimumSize(600, 400);
  QTextEdit *textEdit = new QTextEdit(&dialog);
  textEdit->setPlainText(aboutText);
  textEdit->setReadOnly(true);
  QPushButton *closeButton = new QPushButton("Close", &dialog);
  QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->addWidget(textEdit);
  layout->addWidget(closeButton);
  dialog.exec();
}

void MainWindow::on_actionOpen_triggered()
{
  const QString title = tr("_open_file");
  const QString formats = tr("_images") + " (*.png *.jpg *.jpeg *.bmp *.gif *.json)";
  QString fileName = QFileDialog::getOpenFileName(this, title, "", formats);
  currentFilePath = fileName;
  processFile(currentFilePath);
  ui->verticalTolerance->setValue(extractor->m_maxFrameHeight / 3);
}

void MainWindow::on_actionExit_triggered()
{
  exit(EXIT_SUCCESS);
}

void MainWindow::on_fps_valueChanged(int fps)
{
  ui->timingLabel->setText(" -> " + tr("_timing") + ": " + QString::number(1000.0  / (double)fps, 'g', 4) + "ms");
}

void MainWindow::on_alphaThreshold_valueChanged(int threshold)
{
  Q_UNUSED(threshold);
  if (!currentFilePath.isEmpty()) {
      processFile(currentFilePath);
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

QColor getHighlightColor(int index, int total)
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
  Q_UNUSED(index); // Selection is internaly managed, current index have not real purpose

  if (!ui->graphicsViewLayers->scene() || !extractor) {
      return;
    }

  QModelIndexList selectedIndexes = ui->framesList->selectionModel()->selectedIndexes();

  clearBoundingBoxHighlighters();

  QGraphicsScene *scene = ui->graphicsViewLayers->scene();

  for (int i = 0; i < selectedIndexes.size(); ++i) {
      const QModelIndex &currentIndex = selectedIndexes.at(i);
      int row = currentIndex.row();

      if (row >= 0 && row < extractor->m_atlas_index.size()) {
          const Extractor::Box &box = extractor->m_atlas_index.at(row);
          QRectF rect(box.x, box.y, box.w, box.h);

          QColor color = getHighlightColor(i, selectedIndexes.size());
          color.setAlpha(50);
          QPen pen(Qt::red);
          pen.setWidth(2);
          pen.setStyle(Qt::DashLine);

          QGraphicsRectItem *highlighter = new QGraphicsRectItem(rect);
          highlighter->setPen(pen);
          highlighter->setBrush(QBrush(color));
          highlighter->setToolTip(QString("Frame %1\nPosition: (%2, %3)\nSize: %4x%5")
                                       .arg(row + 1)
                                       .arg(box.x).arg(box.y)
                                       .arg(box.w).arg(box.h));

          QGraphicsSimpleTextItem *label = new QGraphicsSimpleTextItem(QString::number(row + 1), highlighter);
          label->setPos(rect.topLeft());
          label->setBrush(Qt::black);
          label->setFont(QFont("Arial", 10, QFont::Bold));

          scene->addItem(highlighter);
          boundingBoxHighlighters.append(highlighter);
        }
    }

  if (!selectedIndexes.isEmpty()) {
      fitSelectedFramesInView();
    }
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

  if (extension == "json") {
      extractorOut = new JsonExtractor();
    } else if (extension == "png") {
      extractorOut = new SpriteExtractor();
    } else if (extension == "gif") {
      extractorOut = new GifExtractor();
    } else {
      extractorOut = new JsonExtractor();
    }

  extractorOut->exportFrames(filePath, baseName, extractor);
}

void MainWindow::on_Play_clicked()
{
  if (selectedFrameRows.isEmpty()) {
      startAnimation();
      return;
    }

  int fpsValue = ui->fps->value();
  int intervalMs = (fpsValue > 0) ? (1000 / fpsValue) : 100;
  animationTimer->setInterval(intervalMs);

  if (!animationTimer->isActive()) {
      animationTimer->start();
    }

  ui->Pause->setVisible(true);
  ui->Play->setVisible(false);
}

void MainWindow::on_Pause_clicked()
{
  if (animationTimer->isActive()) {
      animationTimer->stop();
    }
  ui->Play->setVisible(true);
  ui->Pause->setVisible(false);

}

