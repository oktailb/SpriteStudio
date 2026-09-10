#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include "extractor/spriteextractor.h"

void MainWindow::reverseFramesOrder(QList<int> &selectedIndices)
{
    if (selectedIndices.isEmpty()) return;

    std::reverse(selectedIndices.begin(), selectedIndices.end());
    syncAnimationListWidget();
    startAnimation();
}

void MainWindow::invertSelection()
{
    if (!extractor) return;

    invertFrameSelection();
}

void MainWindow::invertFrameSelection()
{
    if (!extractor) return;

    QList<int> newSelection;
    for (int i = 0; i < extractor->m_atlas_index.size(); ++i) {
        if (!extractor->m_atlas_index.at(i).selected) {
            newSelection.append(i);
        }
    }

    setSelectedFrameIndices(newSelection);

    clearBoundingBoxHighlighters();
    setBoundingBoxHighlighters(newSelection);
}

void MainWindow::deleteFrame(int row)
{
    if (row < 0 || row >= extractor->m_atlas_index.size()) return;

    extractor->removeFrame(row);
    frameModel->removeRow(row);
    syncAnimationListWidget();
    clearBoundingBoxHighlighters();
}

void MainWindow::deleteSelectedFrame()
{
    QList<int> selectedIndices = getSelectedFrameIndices();

    if (selectedIndices.isEmpty()) return;

    deleteFrames(selectedIndices);
}

void MainWindow::deleteFrames(const QList<int> &frameIndices)
{
    if (frameIndices.isEmpty()) return;

    syncToDocument();
    m_undoStack->push(new DeleteFramesCommand(m_document, frameIndices));
}

void MainWindow::setMergeHighlight(const QModelIndex &index, bool show)
{
  // Preventative cleanup
  if (!show || !index.isValid()) {
      clearMergeHighlight();
      return;
    }

  int row = index.row();
  if (row >= 0 && row < extractor->m_atlas_index.size()) {
      // Get the matching box
      Extractor::Box box = extractor->m_atlas_index[row];
      QRectF rect(box.rect);

      // Rectangle lazy creation
      if (!mergeHighlighter) {
          mergeHighlighter = new QGraphicsRectItem();
          QPen pen(Qt::magenta);
          pen.setWidth(3);
          pen.setStyle(Qt::DotLine);
          mergeHighlighter->setPen(pen);
          // Important : Ensure to be over the image
          mergeHighlighter->setZValue(10);

          if (ui->graphicsViewLayers->scene()) {
              ui->graphicsViewLayers->scene()->addItem(mergeHighlighter);
            }
        }

      // Force reload if the scene have been altered
      if (mergeHighlighter->scene() != ui->graphicsViewLayers->scene()) {
          if (ui->graphicsViewLayers->scene()) {
              ui->graphicsViewLayers->scene()->addItem(mergeHighlighter);
            }
        }

      mergeHighlighter->setRect(rect);
      mergeHighlighter->setVisible(true);
    }
}

void MainWindow::clearMergeHighlight()
{
  if (mergeHighlighter) {
      mergeHighlighter->setVisible(false);
    }
}

void MainWindow::populateFrameList(const QList<QPixmap> &frameList, const QList<Extractor::Box> &boxList)
{
  frameModel->clear();
  clearBoundingBoxHighlighters();

  frameModel->setColumnCount(1);

  if (frameList.size() != boxList.size()) {
      qWarning() << "Incohérence dans populateFrameList: frames =" << frameList.size()
                  << ", boxes =" << boxList.size();
    }

  int itemCount = qMin(frameList.size(), boxList.size());
  progressBar->setValue(0);
  statusLabel->setText(tr("_populating_frame_list"));;

  for (int i = 0; i < itemCount; ++i) {
      const QPixmap &pixmap = frameList.at(i);
      const Extractor::Box &box = boxList.at(i);

      QStandardItem *item = new QStandardItem();

      QPixmap thumbnail = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      item->setData(thumbnail, Qt::DecorationRole);

             // Utiliser i+1 pour l'affichage utilisateur (1-index)
      QString displayText = QString("Frame %1").arg(i + 1);
      if (box.selected) {
          displayText += " ✓";
          item->setBackground(QBrush(QColor(200, 230, 255)));
        }

      item->setData(displayText, Qt::DisplayRole);
      item->setData(i, Qt::UserRole);
      item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
      item->setFlags(item->flags() | Qt::ItemIsEditable);
      frameModel->appendRow(item);
      progressBar->setValue(100*i/itemCount);
    }
  progressBar->setValue(0);
  statusLabel->setText(tr("_ready"));;

  syncAnimationListWidget();
}

QList<int> MainWindow::getSelectedFrameIndices() const
{
    QList<int> selectedIndices;
    if (!extractor) return selectedIndices;

    for (int i = 0; i < extractor->m_atlas_index.size(); ++i) {
        if (extractor->m_atlas_index.at(i).selected) {
            selectedIndices.append(i);
        }
    }
    return selectedIndices;
}

void MainWindow::setSelectedFrameIndices(const QList<int> &selectedIndices)
{
  if (!extractor) return;

  clearFrameSelections();

  for (int index : selectedIndices) {
      if (index >= 0 && index < extractor->m_atlas_index.size()) {
          extractor->m_atlas_index[index].selected = true;
        }
    }

  updateFrameListSelectionFromModel();
  updateCurrentAnimation();
}

void MainWindow::clearFrameSelections()
{
    if (!extractor) return;

    for (Extractor::Box &box : extractor->m_atlas_index) {
        box.selected = false;
    }

    updateFrameListSelectionFromModel();
    updateCurrentAnimation();
}

void MainWindow::updateFrameListSelectionFromModel()
{
  if (!extractor) return;

  QItemSelection selection;
  for (int i = 0; i < extractor->m_atlas_index.size(); ++i) {
      if (extractor->m_atlas_index.at(i).selected) {
          QModelIndex index = frameModel->index(i, 0);
          selection.select(index, index);
        }
    }
  ui->framesList->selectionModel()->select(selection,
                                             QItemSelectionModel::ClearAndSelect);
}

void MainWindow::populateFrameList(const QList<QPixmap> &frameList, const QList<SpriteBox> &boxList)
{
    QList<Extractor::Box> eboxes;
    eboxes.reserve(boxList.size());
    for (const SpriteBox &sb : boxList) {
        Extractor::Box eb;
        eb.rect = sb.rect;
        eb.index = sb.index;
        eb.selected = sb.selected;
        eb.groupId = sb.groupId;
        eb.overlappingBoxes = sb.overlappingBoxes;
        eboxes.append(eb);
    }
    populateFrameList(frameList, eboxes);
}

void MainWindow::syncFromDocument()
{
    if (!m_document) return;

    if (!extractor) {
        extractor = new SpriteExtractor(this);
        connect(extractor, &Extractor::progress, progressBar, &QProgressBar::setValue);
        connect(extractor, &Extractor::statusMessage, statusLabel, &QLabel::setText);
    }

    extractor->m_frames = m_document->frames();
    extractor->m_atlas = m_document->atlas();
    extractor->m_atlas_index.clear();
    for (const SpriteBox &sb : m_document->boxes()) {
        Extractor::Box eb;
        eb.rect = sb.rect;
        eb.index = sb.index;
        eb.selected = sb.selected;
        eb.groupId = sb.groupId;
        eb.overlappingBoxes = sb.overlappingBoxes;
        extractor->m_atlas_index.append(eb);
    }

    extractor->m_animationsData.clear();
    for (auto it = m_document->animations().begin(); it != m_document->animations().end(); ++it) {
        Extractor::AnimationData ad;
        ad.frameIndices = it.value().frameIndices;
        ad.fps = it.value().fps;
        extractor->m_animationsData[it.key()] = ad;
    }

    extractor->m_maxFrameWidth = m_document->maxFrameWidth();
    extractor->m_maxFrameHeight = m_document->maxFrameHeight();

    populateFrameList(extractor->m_frames, extractor->m_atlas_index);
    setupGraphicsView(extractor->m_atlas);
    syncAnimationListWidget();

    if (!m_document->animations().isEmpty()) {
        QString firstAnim = m_document->animations().firstKey();
        QList<QTreeWidgetItem*> items = ui->animationList->findItems(firstAnim, Qt::MatchExactly, 0);
        if (!items.isEmpty()) {
            ui->animationList->setCurrentItem(items.first());
            startAnimation();
        }
    }
}

void MainWindow::syncToDocument()
{
    if (!m_document || !extractor) return;

    QSignalBlocker blocker(m_document);

    m_document->setAtlas(extractor->m_atlas);
    QList<SpriteBox> sboxes;
    sboxes.reserve(extractor->m_atlas_index.size());
    for (const Extractor::Box &eb : extractor->m_atlas_index) {
        SpriteBox sb;
        sb.rect = eb.rect;
        sb.index = eb.index;
        sb.selected = eb.selected;
        sb.groupId = eb.groupId;
        sb.overlappingBoxes = eb.overlappingBoxes;
        sboxes.append(sb);
    }
    m_document->setFrames(extractor->m_frames, sboxes);

    for (auto it = extractor->m_animationsData.begin(); it != extractor->m_animationsData.end(); ++it) {
        m_document->setAnimation(it.key(), it.value().frameIndices, it.value().fps, true);
    }
}

