#include "image/spritedetector.h"
#include "config/appconfig.h"
#include <vector>
#include <algorithm>

namespace {
struct Point2D {
    int x;
    int y;
};
}

bool SpriteDetector::detectToImages(const QImage &sourceImage,
                                    QList<QImage> &outFrames,
                                    QList<SpriteBox> &outBoxes,
                                    const SpriteDetectionOptions &options,
                                    std::function<void(int)> progressCallback)
{
    outFrames.clear();
    outBoxes.clear();

    if (sourceImage.isNull()) {
        return false;
    }

    QImage image = (sourceImage.format() == QImage::Format_ARGB32)
        ? sourceImage
        : sourceImage.convertToFormat(QImage::Format_ARGB32);

    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0) return true;

    const int ALPHA_THRESHOLD = (options.alphaThreshold < 0)
        ? AppConfig::instance().atlas().defaultAlphaThreshold
        : options.alphaThreshold;
    const int verticalTolerance = (options.verticalTolerance < 0)
        ? AppConfig::instance().atlas().defaultVerticalTolerance
        : options.verticalTolerance;

    std::vector<int> componentIdAtPixel(static_cast<size_t>(w * h), -1);
    QList<QRect> componentRects;
    std::vector<Point2D> stack;
    stack.reserve(4096);

    std::vector<const QRgb*> scanLines(h);
    for (int y = 0; y < h; ++y) {
        scanLines[y] = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    }

    for (int y = 0; y < h; ++y) {
        const QRgb *lineY = scanLines[y];
        const int y_w = y * w;
        for (int x = 0; x < w; ++x) {
            const int idx = y_w + x;
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
        if (progressCallback) progressCallback(100);
        return true;
    }

    if (progressCallback) progressCallback(50);

    // Filter components fully contained inside another
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

    // Sort master components in reading order
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

    // Map component -> slice index
    std::vector<int> componentIdToBoxIndex(componentRects.size(), -1);
    for (int i = 0; i < masterComponentIds.size(); ++i) {
        componentIdToBoxIndex[masterComponentIds[i]] = i;
    }

    // Build pixel ownership
    std::vector<int> pixelOwner(static_cast<size_t>(w * h), -1);
    for (int idx = 0; idx < w * h; ++idx) {
        int c = componentIdAtPixel[idx];
        if (c >= 0) {
            pixelOwner[idx] = componentIdToBoxIndex[c];
        }
    }

    // Build bounding boxes & extract frames via direct scanline memory
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

    if (progressCallback) progressCallback(100);
    return true;
}

bool SpriteDetector::detectBoxes(const QImage &sourceImage,
                                 QList<SpriteBox> &outBoxes,
                                 const SpriteDetectionOptions &options,
                                 std::function<void(int)> progressCallback)
{
    QList<QImage> unusedFrames;
    return detectToImages(sourceImage, unusedFrames, outBoxes, options, progressCallback);
}
