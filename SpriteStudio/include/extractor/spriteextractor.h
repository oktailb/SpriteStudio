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

};

#endif // SPRITEEXTRACTOR_H
