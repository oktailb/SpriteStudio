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

  QList<QPixmap> extractFrames(const QString &filePath);

};

#endif // GIFEXTRACTOR_H
