#ifndef JSONEXTRACTOR_H
#define JSONEXTRACTOR_H

#include <QObject>
#include <QPixmap>
#include <QList>
#include <QString>
#include "extractor.h"

class JsonExtractor : public Extractor
{
    Q_OBJECT
public:
    explicit JsonExtractor(QObject *parent = nullptr);

    QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance);
    bool           exportFrames(const QString &basePath, const QString &projectName);
};

#endif // JSONEXTRACTOR_H
