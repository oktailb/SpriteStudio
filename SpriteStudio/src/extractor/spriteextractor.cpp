#include "extractor/spriteextractor.h"
#include <QDebug>
#include <QStack>

SpriteExtractor::SpriteExtractor(QLabel *statusBar, QProgressBar *progressBar, QObject *parent)
    : Extractor(statusBar, progressBar, parent)
{
}

QList<QPixmap> SpriteExtractor::extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance)
{
  QPixmap atlasPixmap(filePath);
  if (atlasPixmap.isNull()) {
      qWarning() << "Erreur: Impossible de charger l'image:" << filePath;
      return m_frames;
    }
  m_atlas = atlasPixmap.toImage();
  m_filePath = filePath;

  return extractFromPixmap(alphaThreshold, verticalTolerance);
}

QList<QPixmap> SpriteExtractor::extractFromPixmap(int alphaThreshold, int verticalTolerance)
{
  m_frames.clear();
  m_atlas_index.clear();
  m_maxFrameWidth = 0;
  m_maxFrameHeight = 0;

  QImage image = m_atlas;
  if (image.format() != QImage::Format_ARGB32 && image.hasAlphaChannel()) {
    image = image.convertToFormat(QImage::Format_ARGB32);
  } else if (!image.hasAlphaChannel()) {
    image = image.convertToFormat(QImage::Format_ARGB32);
  }

  const int w = image.width();
  const int h = image.height();
  const int ALPHA_THRESHOLD = alphaThreshold;

  // 1) Flood-fill : assigner un id de composante à chaque pixel, et calculer la bbox par composante
  QVector<int> componentIdAtPixel(w * h, -1);
  QList<QRect> componentRects;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      int idx = y * w + x;
      if (componentIdAtPixel[idx] >= 0 || qAlpha(image.pixel(x, y)) <= ALPHA_THRESHOLD)
        continue;

      const int componentId = componentRects.size();
      int minX = w, minY = h, maxX = -1, maxY = -1;

      QStack<QPoint> stack;
      stack.push(QPoint(x, y));
      componentIdAtPixel[idx] = componentId;

      while (!stack.isEmpty()) {
        QPoint p = stack.pop();
        int cx = p.x(), cy = p.y();
        minX = qMin(minX, cx);
        minY = qMin(minY, cy);
        maxX = qMax(maxX, cx);
        maxY = qMax(maxY, cy);

        const int dx[] = {0, 0, 1, -1};
        const int dy[] = {1, -1, 0, 0};
        for (int i = 0; i < 4; ++i) {
          int nx = cx + dx[i], ny = cy + dy[i];
          if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
          int nidx = ny * w + nx;
          if (componentIdAtPixel[nidx] >= 0 || qAlpha(image.pixel(nx, ny)) <= ALPHA_THRESHOLD)
            continue;
          componentIdAtPixel[nidx] = componentId;
          stack.push(QPoint(nx, ny));
        }
      }

      if (minX <= maxX && minY <= maxY)
        componentRects.append(QRect(minX, minY, maxX - minX + 1, maxY - minY + 1));
    }
  }

  if (componentRects.isEmpty()) {
    emit extractionFinished(0);
    return m_frames;
  }

  // 2) Filtrer les boîtes entièrement contenues dans une autre (garder les "master" seulement)
  QList<bool> isMaster(componentRects.size(), true);
  for (int i = 0; i < componentRects.size(); ++i) {
    for (int j = 0; j < componentRects.size(); ++j) {
      if (i == j) continue;
      if (componentRects[j].contains(componentRects[i])) {
        isMaster[i] = false;
        break;
      }
    }
  }

  // 3) Liste des composantes "master" triée par position (ordre lecture)
  QList<int> masterComponentIds;
  for (int c = 0; c < componentRects.size(); ++c)
    if (isMaster[c]) masterComponentIds.append(c);

  std::sort(masterComponentIds.begin(), masterComponentIds.end(),
            [&componentRects, verticalTolerance](int a, int b) {
              const QRect &ra = componentRects[a], &rb = componentRects[b];
              if (qAbs(ra.y() - rb.y()) <= verticalTolerance)
                return ra.x() < rb.x();
              return ra.y() < rb.y();
            });

  // 4) componentId -> index de box dans m_atlas_index (ordre d'affichage)
  QVector<int> componentIdToBoxIndex(componentRects.size(), -1);
  for (int i = 0; i < masterComponentIds.size(); ++i)
    componentIdToBoxIndex[masterComponentIds[i]] = i;

  // 5) Carte pixel -> index de box (0..n-1) ou -1 (fond / composante ignorée)
  QVector<int> pixelOwner(w * h, -1);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      int c = componentIdAtPixel[y * w + x];
      if (c >= 0)
        pixelOwner[y * w + x] = componentIdToBoxIndex[c];
    }
  }

  // 6) Construire m_atlas_index
  m_atlas_index.clear();
  for (int i = 0; i < masterComponentIds.size(); ++i) {
    Box box;
    box.rect = componentRects[masterComponentIds[i]];
    box.index = i;
    box.selected = false;
    box.groupId = -1;
    box.overlappingBoxes.clear();
    m_atlas_index.append(box);
  }

  // 7) Extraction : pour chaque box, copier le rect puis masquer les pixels "étrangers"
  for (int i = 0; i < m_atlas_index.size(); ++i) {
    const QRect &rect = m_atlas_index[i].rect;
    QImage frame = image.copy(rect);
    for (int ly = 0; ly < frame.height(); ++ly) {
      for (int lx = 0; lx < frame.width(); ++lx) {
        int gx = rect.x() + lx;
        int gy = rect.y() + ly;
        if (pixelOwner[gy * w + gx] != i)
          frame.setPixel(lx, ly, 0); // transparent
      }
    }
    QPixmap spriteFrame = QPixmap::fromImage(frame);
    addFrame(spriteFrame);
    m_maxFrameWidth = qMax(m_maxFrameWidth, spriteFrame.width());
    m_maxFrameHeight = qMax(m_maxFrameHeight, spriteFrame.height());
  }

  emit extractionFinished(m_frames.size());
  return m_frames;
}

bool SpriteExtractor::exportFrames(const QString &basePath, const QString &projectName, Extractor *in)
{
  Q_UNUSED(basePath);
  Q_UNUSED(projectName);
  Q_UNUSED(in);

  return false;
}
