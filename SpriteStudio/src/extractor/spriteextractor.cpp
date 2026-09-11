#include "extractor/spriteextractor.h"
#include <QDebug>
#include <QStack>
#include <QFileInfo>
#include <QDir>
#include <algorithm>

SpriteExtractor::SpriteExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool SpriteExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    return supportedExtensions().contains(fi.suffix().toLower());
}

bool SpriteExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    setStatusMessage(tr("Loading image %1...").arg(QFileInfo(filePath).fileName()));
    setProgress(5);

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("File not found: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QImage image(filePath);
    if (image.isNull()) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Failed to decode image from: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    bool ok = extractFromImage(image, outDoc, m_options);
    if (ok) {
        outDoc.setFilePath(filePath);
    }
    return ok;
}

bool SpriteExtractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(options);
    if (inDoc.atlas().isNull()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Cannot export: Document atlas image is null.");
            error->filePath = filePath;
        }
        return false;
    }

    QFileInfo fi(filePath);
    QString format = fi.suffix().toUpper();
    if (format.isEmpty()) format = QStringLiteral("PNG");

    if (!inDoc.atlas().save(filePath, format.toLatin1().constData())) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to save image to: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    setStatusMessage(tr("Exported atlas image to: %1").arg(filePath));
    return true;
}

bool SpriteExtractor::extractFromImage(const QImage &image, SpriteDocument &outDoc, int alphaThreshold, int verticalTolerance)
{
    SpriteSheetOptions opts = m_options;
    opts.alphaThreshold = alphaThreshold;
    opts.verticalTolerance = verticalTolerance;
    return extractFromImage(image, outDoc, opts);
}

bool SpriteExtractor::extractFromImage(const QImage &sourceImage, SpriteDocument &outDoc, const SpriteSheetOptions &options)
{
    setStatusMessage(tr("Segmenting sprite frames..."));
    setProgress(15);

    QImage image = sourceImage;
    if (image.format() != QImage::Format_ARGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    const int w = image.width();
    const int h = image.height();
    const int ALPHA_THRESHOLD = options.alphaThreshold;
    const int verticalTolerance = options.verticalTolerance;

    // 1) Flood-fill component identification
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

            if (minX <= maxX && minY <= maxY) {
                componentRects.append(QRect(minX, minY, maxX - minX + 1, maxY - minY + 1));
            }
        }
    }

    if (componentRects.isEmpty()) {
        outDoc.setAtlas(image);
        outDoc.setFrames({}, {});
        setProgress(100);
        emit extractionFinished(0);
        return true;
    }

    setProgress(50);

    // 2) Filter components fully contained inside another
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

    // 3) Sort master components in reading order (top-to-bottom, left-to-right)
    QList<int> masterComponentIds;
    for (int c = 0; c < componentRects.size(); ++c) {
        if (isMaster[c]) masterComponentIds.append(c);
    }

    std::sort(masterComponentIds.begin(), masterComponentIds.end(),
              [&componentRects, verticalTolerance](int a, int b) {
                  const QRect &ra = componentRects[a], &rb = componentRects[b];
                  if (qAbs(ra.y() - rb.y()) <= verticalTolerance)
                      return ra.x() < rb.x();
                  return ra.y() < rb.y();
              });

    // 4) Map component -> slice index
    QVector<int> componentIdToBoxIndex(componentRects.size(), -1);
    for (int i = 0; i < masterComponentIds.size(); ++i) {
        componentIdToBoxIndex[masterComponentIds[i]] = i;
    }

    // 5) Build pixel ownership
    QVector<int> pixelOwner(w * h, -1);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int c = componentIdAtPixel[y * w + x];
            if (c >= 0)
                pixelOwner[y * w + x] = componentIdToBoxIndex[c];
        }
    }

    // 6) Build bounding boxes & extract frames
    QList<SpriteBox> boxes;
    QList<QPixmap> frames;
    boxes.reserve(masterComponentIds.size());
    frames.reserve(masterComponentIds.size());

    for (int i = 0; i < masterComponentIds.size(); ++i) {
        SpriteBox box;
        box.rect = componentRects[masterComponentIds[i]];
        box.index = i;
        box.selected = false;
        box.groupId = -1;
        boxes.append(box);

        QImage frame = image.copy(box.rect);
        for (int ly = 0; ly < frame.height(); ++ly) {
            for (int lx = 0; lx < frame.width(); ++lx) {
                int gx = box.rect.x() + lx;
                int gy = box.rect.y() + ly;
                if (pixelOwner[gy * w + gx] != i) {
                    frame.setPixel(lx, ly, 0); // transparent
                }
            }
        }
        frames.append(QPixmap::fromImage(frame));
    }

    // 7) Populate document directly
    outDoc.setAtlas(image);
    outDoc.setFrames(frames, boxes);

    setProgress(100);
    setStatusMessage(tr("Extracted %1 frames").arg(frames.size()));
    emit extractionFinished(frames.size());
    return true;
}
