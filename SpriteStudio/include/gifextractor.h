#ifndef GIFEXTRACTOR_H
#define GIFEXTRACTOR_H

#include <QObject>
#include <QPixmap>
#include <QList>
#include <QString>

#include "extractor.h"

class GifExtractor : public Extractor
{
  Q_OBJECT
public:
  explicit GifExtractor(QObject *parent = nullptr);

    QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance);
    QList<QPixmap> extractFromPixmap(int alphaThreshold, int verticalTolerance);
    bool           exportFrames(const QString &basePath, const QString &projectName, Extractor* in);
};

#endif // GIFEXTRACTOR_H
