#include "spriteextractor.h"
#include <QDebug>
#include <QImage>
#include <QStack>
#include <QPoint>
#include <QRect>
#include <algorithm>
#include <cmath>

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
  // On délègue le travail à la méthode qui traite la pixmap
  return extractFromPixmap(atlasPixmap, alphaThreshold, verticalTolerance);
}

QList<QPixmap> SpriteExtractor::extractFromPixmap(QPixmap atlasPixmap, int alphaThreshold, int verticalTolerance)
{
  m_frames.clear();
  m_atlas_index.clear();

         // Important : on stocke l'atlas courant (qui peut être celui modifié avec transparence)
  m_atlas = atlasPixmap;

  QImage image = m_atlas.toImage();

  if (image.format() != QImage::Format_ARGB32 && image.hasAlphaChannel()) {
      image = image.convertToFormat(QImage::Format_ARGB32);
    } else if (!image.hasAlphaChannel()) {
      // Force le canal Alpha si c'est un BMP/JPG
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

                  Extractor::Box box = {minX, minY, spriteW, spriteH};
                  detectedBoxes.push_back(box);
                }
            }
        }
    }

  std::sort( detectedBoxes.begin(),
             detectedBoxes.end(),
             [&verticalTolerance](const Extractor::Box& a, const Extractor::Box& b) {
               const int VERTICAL_TOLERANCE = verticalTolerance;
               if (std::abs(a.y - b.y) <= VERTICAL_TOLERANCE) {
                   return a.x < b.x;
                 }
               return a.y < b.y;
             });

  if (detectedBoxes.size() > 0) {
      QList<QRect> rects;
      for (const auto& box : detectedBoxes) {
          rects.append(QRect(box.x, box.y, box.w, box.h));
        }

      QList<QRect> masterRects;
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
              m_atlas_index.push_back({rect.x(), rect.y(), rect.width(), rect.height()});
            }
        }
    }

  for (const auto& box : m_atlas_index) {
      QPixmap spriteFrame = atlasPixmap.copy(box.x, box.y, box.w, box.h);
      this->addFrame(spriteFrame);
      if (spriteFrame.width() > maxFrameWidth) {
          maxFrameWidth = spriteFrame.width();
        }
      if (spriteFrame.height() > maxFrameHeight) {
          maxFrameHeight = spriteFrame.height();
        }
    }

  qDebug() << "Extraction mémoire terminée. Frames:" << m_frames.size();
  emit extractionFinished(m_frames.size());
  return m_frames;
}

bool SpriteExtractor::exportFrames(const QString &basePath, const QString &projectName, Extractor *in)
{
  return false;
}
