#include "spriteextractor.h"
#include <QDebug>
#include <QStack>

SpriteExtractor::SpriteExtractor(QObject *parent) : Extractor(parent)
{
}

QList<QPixmap> SpriteExtractor::extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance)
{
  QPixmap atlasPixmap(filePath);
  if (atlasPixmap.isNull()) {
      qWarning() << "Erreur: Impossible de charger l'image:" << filePath;
      return m_frames;
    }
  m_atlas = atlasPixmap;
  m_filePath = filePath;

  return extractFromPixmap(alphaThreshold, verticalTolerance);
}

QList<QPixmap> SpriteExtractor::extractFromPixmap(int alphaThreshold, int verticalTolerance)
{
  m_frames.clear();
  m_atlas_index.clear();

  m_maxFrameWidth = 0;
  m_maxFrameHeight = 0;

  QImage image = m_atlas.toImage();

  if (image.format() != QImage::Format_ARGB32 && image.hasAlphaChannel()) {
      image = image.convertToFormat(QImage::Format_ARGB32);
    } else if (!image.hasAlphaChannel()) {
      // Force Alpha layer if BMP/JPG
      image = image.convertToFormat(QImage::Format_ARGB32);
    }

  int w = image.width();
  int h = image.height();

  const int ALPHA_THRESHOLD = alphaThreshold;
  QVector<QVector<bool>> visited(h, QVector<bool>(w, false));
  QList<Extractor::Box> detectedBoxes;

  for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
          if (!visited[y][x] && qAlpha(image.pixel(x, y)) > ALPHA_THRESHOLD) {
              int minX = w;
              int minY = h;
              int maxX = -1;
              int maxY = -1;

              QStack<QPoint> stack;
              stack.push(QPoint(x, y));
              visited[y][x] = true;

              while (!stack.isEmpty()) {
                  QPoint current = stack.pop();
                  int curX = current.x();
                  int curY = current.y();

                  minX = qMin(minX, curX);
                  minY = qMin(minY, curY);
                  maxX = qMax(maxX, curX);
                  maxY = qMax(maxY, curY);

                  int dx[] = {0, 0, 1, -1};
                  int dy[] = {1, -1, 0, 0};

                  for (int i = 0; i < 4; ++i) {
                      int nextX = curX + dx[i];
                      int nextY = curY + dy[i];

                      if (nextX >= 0 && nextX < w && nextY >= 0 && nextY < h) {
                          if (!visited[nextY][nextX] && qAlpha(image.pixel(nextX, nextY)) > ALPHA_THRESHOLD) {
                              visited[nextY][nextX] = true;
                              stack.push(QPoint(nextX, nextY));
                            }
                        }
                    }
                }

              if (minX <= maxX && minY <= maxY) {
                  int spriteW = maxX - minX + 1;
                  int spriteH = maxY - minY + 1;

                  Box box;
                  box.rect = {minX, minY, spriteW, spriteH};
                  box.index  = 0;
                  box.selected = false;
                  detectedBoxes.push_back(box);
                }
            }
        }
    }

  std::sort( detectedBoxes.begin(),
             detectedBoxes.end(),
             [&verticalTolerance](const Extractor::Box& a, const Extractor::Box& b) {
               const int VERTICAL_TOLERANCE = verticalTolerance;
               if (std::abs(a.rect.y() - b.rect.y()) <= VERTICAL_TOLERANCE) {
                   return a.rect.x() < b.rect.x();
                 }
               return a.rect.y() < b.rect.y();
             });

  if (detectedBoxes.size() > 0) {
      QList<QRect> rects;
      for (const auto& box : detectedBoxes) {
          rects.append(box.rect);
        }

      QList<bool> isMaster(rects.size(), true);

      for (int i = 0; i < rects.size(); ++i) {
          for (int j = 0; j < rects.size(); ++j) {
              if (i == j) continue;

              if (rects[j].contains(rects[i])) {
                  isMaster[i] = false;
                  break;
                }
            }
        }

      m_atlas_index.clear();
      for (int i = 0; i < rects.size(); ++i) {
          if (isMaster[i]) {
              QRect rect = rects[i];
              Box box;
              box.rect = rect;
              box.index = i;
              box.selected = false;
              m_atlas_index.push_back(box);
            }
        }
    }

  for (const auto& box : m_atlas_index) {
      QPixmap spriteFrame = m_atlas.copy(box.rect);
      this->addFrame(spriteFrame);
      if (spriteFrame.width() > m_maxFrameWidth) {
          m_maxFrameWidth = spriteFrame.width();
        }
      if (spriteFrame.height() > m_maxFrameHeight) {
          m_maxFrameHeight = spriteFrame.height();
        }
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
