#include "extractor/spriteextractor.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <vector>

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

bool SpriteExtractor::extractToImages(const QImage &sourceImage,
                                     QList<QImage> &outFrames,
                                     QList<SpriteBox> &outBoxes,
                                     const SpriteSheetOptions &options)
{
    setStatusMessage(tr("Segmenting sprite frames..."));
    setProgress(15);

    QImage image = sourceImage;
    if (image.format() != QImage::Format_ARGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0) {
        outFrames.clear();
        outBoxes.clear();
        return false;
    }

    const int ALPHA_THRESHOLD = options.alphaThreshold;
    const int verticalTolerance = options.verticalTolerance;

    // Fast direct scanline pointers
    std::vector<const QRgb*> scanLines(h);
    for (int y = 0; y < h; ++y) {
        scanLines[y] = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    }

    // 1) Flood-fill component identification with contiguous 1D array & fast stack
    std::vector<int> componentIdAtPixel(static_cast<size_t>(w * h), -1);
    QList<QRect> componentRects;

    struct Point2D { int x; int y; };
    std::vector<Point2D> stack;
    stack.reserve(8192);

    for (int y = 0; y < h; ++y) {
        const QRgb *lineY = scanLines[y];
        const int y_w = y * w;
        for (int x = 0; x < w; ++x) {
            int idx = y_w + x;
            if (componentIdAtPixel[idx] >= 0 || qAlpha(lineY[x]) <= ALPHA_THRESHOLD)
                continue;

            const int componentId = static_cast<int>(componentRects.size());
            int minX = w, minY = h, maxX = -1, maxY = -1;

            stack.clear();
            stack.push_back({x, y});
            componentIdAtPixel[idx] = componentId;

            while (!stack.empty()) {
                Point2D p = stack.back();
                stack.pop_back();
                int cx = p.x, cy = p.y;
                if (cx < minX) minX = cx;
                if (cy < minY) minY = cy;
                if (cx > maxX) maxX = cx;
                if (cy > maxY) maxY = cy;

                // 4-neighborhood with boundary checks
                // Up
                if (cy > 0) {
                    int ny = cy - 1, nx = cx;
                    int nidx = ny * w + nx;
                    if (componentIdAtPixel[nidx] < 0 && qAlpha(scanLines[ny][nx]) > ALPHA_THRESHOLD) {
                        componentIdAtPixel[nidx] = componentId;
                        stack.push_back({nx, ny});
                    }
                }
                // Down
                if (cy + 1 < h) {
                    int ny = cy + 1, nx = cx;
                    int nidx = ny * w + nx;
                    if (componentIdAtPixel[nidx] < 0 && qAlpha(scanLines[ny][nx]) > ALPHA_THRESHOLD) {
                        componentIdAtPixel[nidx] = componentId;
                        stack.push_back({nx, ny});
                    }
                }
                // Left
                if (cx > 0) {
                    int ny = cy, nx = cx - 1;
                    int nidx = ny * w + nx;
                    if (componentIdAtPixel[nidx] < 0 && qAlpha(scanLines[ny][nx]) > ALPHA_THRESHOLD) {
                        componentIdAtPixel[nidx] = componentId;
                        stack.push_back({nx, ny});
                    }
                }
                // Right
                if (cx + 1 < w) {
                    int ny = cy, nx = cx + 1;
                    int nidx = ny * w + nx;
                    if (componentIdAtPixel[nidx] < 0 && qAlpha(scanLines[ny][nx]) > ALPHA_THRESHOLD) {
                        componentIdAtPixel[nidx] = componentId;
                        stack.push_back({nx, ny});
                    }
                }
            }

            if (minX <= maxX && minY <= maxY) {
                componentRects.append(QRect(minX, minY, maxX - minX + 1, maxY - minY + 1));
            }
        }
    }

    if (componentRects.isEmpty()) {
        outFrames.clear();
        outBoxes.clear();
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
    std::vector<int> componentIdToBoxIndex(componentRects.size(), -1);
    for (int i = 0; i < masterComponentIds.size(); ++i) {
        componentIdToBoxIndex[masterComponentIds[i]] = i;
    }

    // 5) Build pixel ownership
    std::vector<int> pixelOwner(static_cast<size_t>(w * h), -1);
    for (int idx = 0; idx < w * h; ++idx) {
        int c = componentIdAtPixel[idx];
        if (c >= 0) {
            pixelOwner[idx] = componentIdToBoxIndex[c];
        }
    }

    // 6) Build bounding boxes & extract frames via direct scanline memory
    outBoxes.clear();
    outFrames.clear();
    outBoxes.reserve(masterComponentIds.size());
    outFrames.reserve(masterComponentIds.size());

    for (int i = 0; i < masterComponentIds.size(); ++i) {
        SpriteBox box;
        box.rect = componentRects[masterComponentIds[i]];
        box.index = i;
        box.selected = false;
        box.groupId = -1;
        outBoxes.append(box);

        QImage frame = image.copy(box.rect);
        const int fw = frame.width();
        const int fh = frame.height();
        const int bx = box.rect.x();
        const int by = box.rect.y();

        for (int ly = 0; ly < fh; ++ly) {
            QRgb *frameLine = reinterpret_cast<QRgb*>(frame.scanLine(ly));
            const int gy_w = (by + ly) * w;
            for (int lx = 0; lx < fw; ++lx) {
                if (pixelOwner[gy_w + (bx + lx)] != i) {
                    frameLine[lx] = 0; // transparent
                }
            }
        }
        outFrames.append(frame);
    }

    setProgress(100);
    setStatusMessage(tr("Extracted %1 frames").arg(outFrames.size()));
    emit extractionFinished(outFrames.size());
    return true;
}

bool SpriteExtractor::extractFromImage(const QImage &sourceImage, SpriteDocument &outDoc, const SpriteSheetOptions &options)
{
    QList<QImage> frameImages;
    QList<SpriteBox> boxes;

    if (!extractToImages(sourceImage, frameImages, boxes, options)) {
        return false;
    }

    QList<QPixmap> frames;
    frames.reserve(frameImages.size());
    for (const QImage &img : frameImages) {
        frames.append(QPixmap::fromImage(img));
    }

    QImage atlasImg = (sourceImage.format() == QImage::Format_ARGB32)
        ? sourceImage
        : sourceImage.convertToFormat(QImage::Format_ARGB32);

    outDoc.setAtlas(atlasImg);
    outDoc.setFrames(frames, boxes);
    return true;
}
