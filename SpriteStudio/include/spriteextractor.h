#ifndef SPRITEEXTRACTOR_H
#define SPRITEEXTRACTOR_H

#include <QObject>
#include <QPixmap>
#include <QList>
#include <QString>
#include "extractor.h"

/**
 * @brief Extractor derivated class for Sprite Atlas (many aprites on the same picture).
 *
 */
class SpriteExtractor : public Extractor
{
    Q_OBJECT
public:
    explicit SpriteExtractor(QObject *parent = nullptr);

    QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance);
    QList<QPixmap> extractFromPixmap(int alphaThreshold, int verticalTolerance);
    bool           exportFrames(const QString &basePath, const QString &projectName, Extractor* in);
};

#endif // SPRITEEXTRACTOR_H
