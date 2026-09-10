#ifndef ATLASPACKER_H
#define ATLASPACKER_H

#include <QImage>
#include <QPixmap>
#include <QList>
#include <QRect>
#include <QSize>

/**
 * @brief Represents the result of an atlas packing operation.
 */
struct AtlasPackResult {
    QImage          atlas;
    QList<QRect>    frameRects;
    QSize           dimensions;
    bool            success = false;
};

/**
 * @brief The AtlasPacker class provides standalone atlas packing algorithms.
 *
 * It takes a list of frames, packs them tightly into a single composite atlas image,
 * and computes the local bounding rectangle of each frame within the packed atlas.
 */
class AtlasPacker
{
public:
    enum Algorithm {
        RowPacker,       // Fast row-by-row shelf packer
        GridPacker,      // Uniform grid packer
        PowerOfTwoPacker // Row packing rounded up to next power-of-two texture dimensions
    };

    /**
     * @brief Packs a list of pixmaps into a single atlas image.
     * @param frames The frames to pack.
     * @param padding Pixel spacing between frames.
     * @param algo The packing algorithm to use.
     * @return AtlasPackResult containing the atlas image and frame rectangles.
     */
    static AtlasPackResult pack(const QList<QPixmap> &frames, int padding = 2, Algorithm algo = RowPacker);

    /**
     * @brief Packs only the frames referenced by a specific animation.
     * @param allFrames The global list of frames.
     * @param frameIndices The indices of frames belonging to the animation.
     * @param padding Pixel spacing between frames.
     * @return AtlasPackResult containing the animation's atlas and rectangles.
     */
    static AtlasPackResult packIndices(const QList<QPixmap> &allFrames, const QList<int> &frameIndices, int padding = 2);

private:
    static int nextPowerOfTwo(int n);
};

#endif // ATLASPACKER_H
