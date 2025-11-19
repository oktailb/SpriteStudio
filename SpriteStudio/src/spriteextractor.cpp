#include "spriteextractor.h"
#include <QDebug>
#include <QImage>
#include <QStack>

SpriteExtractor::SpriteExtractor(QObject *parent) : Extractor(parent)
{
}

QList<QPixmap> SpriteExtractor::extractFrames(const QString &filePath)
{
    m_frames.clear();
    m_atlas_index.clear();

    QPixmap atlasPixmap(filePath);

    if (atlasPixmap.isNull()) {
        qWarning() << "Erreur: Impossible de charger l'image:" << filePath;
        return m_frames;
    }

    QImage image = atlasPixmap.toImage();

    if (image.format() != QImage::Format_ARGB32 && image.hasAlphaChannel()) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    int w = image.width();
    int h = image.height();

    QVector<QVector<bool>> visited(h, QVector<bool>(w, false));
    m_atlas = atlasPixmap;
    QList<Extractor::Box> detectedBoxes;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!visited[y][x] && qAlpha(image.pixel(x, y)) > 0) {
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
                            if (!visited[nextY][nextX] && qAlpha(image.pixel(nextX, nextY)) > 0) {
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

    if (detectedBoxes.size() > 1) {
        QList<QRect> rects;
        for (const auto& box : detectedBoxes) {
            rects.append(QRect(box.x, box.y, box.w, box.h));
        }

        QList<QRect> finalRects;
        QList<bool> mergedFlags(rects.size(), false);

        for (int i = 0; i < rects.size(); ++i) {
            if (mergedFlags[i]) continue;

            QRect currentRect = rects[i];
            bool isSatellite = false;
            int containingIndex = -1;

            for (int j = 0; j < rects.size(); ++j) {
                if (i == j || mergedFlags[j]) continue;

                QRect otherRect = rects[j];

                if (otherRect.contains(currentRect)) {
                    isSatellite = true;
                    containingIndex = j;
                    break;
                }
            }

            if (isSatellite) {
                mergedFlags[i] = true;
            } else {
                finalRects.append(currentRect);
                mergedFlags[i] = true;
            }
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

        for (int i = 0; i < rects.size(); ++i) {
            if (isMaster[i]) {
                masterRects.append(rects[i]);
            }
        }

        m_atlas_index.clear();
        for (const auto& rect : masterRects) {
            m_atlas_index.push_back({rect.x(), rect.y(), rect.width(), rect.height()});
        }

    } else if (detectedBoxes.size() == 1) {
        m_atlas_index = detectedBoxes;
    }

    for (const auto& box : m_atlas_index) {
        QPixmap spriteFrame = atlasPixmap.copy(box.x, box.y, box.w, box.h);
        this->addFrame(spriteFrame);
    }

    qDebug() << "Extraction du sprite sheet terminée. Frames trouvées:" << m_frames.size();
    emit extractionFinished(m_frames.size());
    return m_frames;
}
