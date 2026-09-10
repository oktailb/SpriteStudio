#include "packer/atlaspacker.h"
#include <QPainter>
#include <cmath>
#include <algorithm>

int AtlasPacker::nextPowerOfTwo(int n)
{
    if (n <= 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

AtlasPackResult AtlasPacker::pack(const QList<QPixmap> &frames, int padding, Algorithm algo)
{
    AtlasPackResult result;
    if (frames.isEmpty()) {
        return result;
    }

    // 1. Calculate total pixel area and max frame width to estimate a target atlas width
    int totalArea = 0;
    int maxFrameW = 0;
    int maxFrameH = 0;

    for (const QPixmap &pix : frames) {
        int w = pix.width() + padding * 2;
        int h = pix.height() + padding * 2;
        totalArea += w * h;
        if (w > maxFrameW) maxFrameW = w;
        if (h > maxFrameH) maxFrameH = h;
    }

    // Target roughly a square aspect ratio
    int sideEstimate = static_cast<int>(std::sqrt(totalArea * 1.25));
    int targetWidth = std::max(sideEstimate, maxFrameW);
    if (algo == PowerOfTwoPacker) {
        targetWidth = nextPowerOfTwo(targetWidth);
    }

    // 2. Shelf / Row packing simulation to determine final atlas dimensions
    int currentX = padding;
    int currentY = padding;
    int rowHeight = 0;
    int maxUsedWidth = 0;

    QList<QRect> computedRects;
    computedRects.reserve(frames.size());

    for (const QPixmap &pix : frames) {
        int fw = pix.width();
        int fh = pix.height();

        if (currentX + fw + padding > targetWidth && currentX > padding) {
            // New shelf
            currentX = padding;
            currentY += rowHeight + padding;
            rowHeight = 0;
        }

        QRect rect(currentX, currentY, fw, fh);
        computedRects.append(rect);

        currentX += fw + padding;
        if (currentX > maxUsedWidth) {
            maxUsedWidth = currentX;
        }
        if (fh > rowHeight) {
            rowHeight = fh;
        }
    }

    int finalWidth = (algo == PowerOfTwoPacker) ? targetWidth : std::max(maxUsedWidth, maxFrameW + padding * 2);
    int finalHeight = currentY + rowHeight + padding;
    if (algo == PowerOfTwoPacker) {
        finalHeight = nextPowerOfTwo(finalHeight);
    }

    // 3. Render atlas
    QImage atlasImage(finalWidth, finalHeight, QImage::Format_ARGB32_Premultiplied);
    atlasImage.fill(Qt::transparent);

    QPainter painter(&atlasImage);
    for (int i = 0; i < frames.size(); ++i) {
        painter.drawPixmap(computedRects[i].topLeft(), frames[i]);
    }
    painter.end();

    result.atlas = atlasImage;
    result.frameRects = computedRects;
    result.dimensions = QSize(finalWidth, finalHeight);
    result.success = true;

    return result;
}

AtlasPackResult AtlasPacker::packIndices(const QList<QPixmap> &allFrames, const QList<int> &frameIndices, int padding)
{
    AtlasPackResult result;
    if (frameIndices.isEmpty() || allFrames.isEmpty()) {
        return result;
    }

    QList<QPixmap> subset;
    subset.reserve(frameIndices.size());
    for (int idx : frameIndices) {
        if (idx >= 0 && idx < allFrames.size()) {
            subset.append(allFrames[idx]);
        }
    }

    return pack(subset, padding, RowPacker);
}
