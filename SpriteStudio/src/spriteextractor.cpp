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
                    m_atlas_index.push_back(box);

                    QPixmap spriteFrame = atlasPixmap.copy(minX, minY, spriteW, spriteH);
                    this->addFrame(spriteFrame);
                }
            }
        }
    }

    qDebug() << "Extraction du sprite sheet terminée. Frames trouvées:" << m_frames.size();
    emit extractionFinished(m_frames.size());
    return m_frames;
}
