#ifndef SPRITEEXTRACTOR_H
#define SPRITEEXTRACTOR_H

#include "extractor/extractor.h"

/**
 * @brief Extractor derivated class for Sprite Atlas (many aprites on the same picture).
 *
 */
class SpriteExtractor : public Extractor
{
    Q_OBJECT
public:
    explicit SpriteExtractor(QLabel *statusBar, QProgressBar *progressBar, QObject *parent = nullptr);

    QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance) override;
    QList<QPixmap> extractFromPixmap(int alphaThreshold, int verticalTolerance) override;
    bool           exportFrames(const QString &basePath, const QString &projectName, Extractor* in) override;

private:
    void detectAndResolveOverlaps(QList<Box>& boxes,
                                  const QImage& image,
                                  int alphaThreshold,
                                  double overlapThreshold = 0.1);
    void resolveOverlappingGroups(QList<Box>& boxes,
                                  const QVector<int>& groupAssigned,
                                  const QImage& image,
                                  int alphaThreshold);
    QList<Box> separateOverlappingSprites(const QList<Box>& boxes,
                                          const QList<int>& groupIndices,
                                          const QImage& image,
                                          int alphaThreshold);
    QRect calculateGroupBounds(const QList<Box>& boxes, const QList<int>& indices);
    void assignGroup(QList<Box>& boxes, QVector<int>& groupAssigned, int currentIndex, int groupId);
    QImage extractGroupMask(const QImage& sourceImage,
                            const QRect& groupBounds,
                            const QList<Box>& boxes,
                            const QList<int>& groupIndices,
                            int alphaThreshold);
    QList<SpriteExtractor::Box> extractConnectedComponentsFromMask(const QImage& mask, const QRect& groupBounds, int alphaThreshold);
    double calculateOverlapRatio(const Box& a, const Box& b);
};

#endif // SPRITEEXTRACTOR_H
