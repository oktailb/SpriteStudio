#ifndef SPRITEEXTRACTOR_H
#define SPRITEEXTRACTOR_H

#include <QObject>
#include <QPixmap>
#include <QList>
#include <QString>
#include "extractor.h"

class SpriteExtractor : public Extractor
{
    Q_OBJECT
public:
    explicit SpriteExtractor(QObject *parent = nullptr);

    QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance);
    bool           exportFrames(const QString &basePath, const QString &projectName);
};

#endif // SPRITEEXTRACTOR_H
