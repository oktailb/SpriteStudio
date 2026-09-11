#ifndef SPRITEDETECTOR_H
#define SPRITEDETECTOR_H

#include <QImage>
#include <QList>
#include <QRect>
#include <functional>
#include "model/spritedocument.h"

/**
 * @brief Configuration parameters for sprite detection and automatic bounding box slicing.
 */
struct SpriteDetectionOptions
{
    int alphaThreshold = 1;
    int verticalTolerance = 0;
    int minSliceSize = 3;
    bool smartCrop = false;
    double overlapThreshold = 0.5;
};

/**
 * @brief High-performance image segmentation engine for detecting and slicing sprites.
 *
 * Uses direct scanline memory access, 1D flat indexing, and connected components flood-fill.
 * Separated from codecs to adhere to the Single Responsibility Principle.
 */
class SpriteDetector
{
public:
    /**
     * @brief Segments an image into sub-images and bounding boxes.
     */
    static bool detectToImages(const QImage &sourceImage,
                               QList<QImage> &outFrames,
                               QList<SpriteBox> &outBoxes,
                               const SpriteDetectionOptions &options = SpriteDetectionOptions(),
                               std::function<void(int)> progressCallback = nullptr);

    /**
     * @brief Segments an image and returns only the bounding boxes.
     */
    static bool detectBoxes(const QImage &sourceImage,
                            QList<SpriteBox> &outBoxes,
                            const SpriteDetectionOptions &options = SpriteDetectionOptions(),
                            std::function<void(int)> progressCallback = nullptr);
};

#endif // SPRITEDETECTOR_H
