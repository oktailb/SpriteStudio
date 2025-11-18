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

  QList<QPixmap> extractFrames(const QString &filePath);
};

#endif // SPRITEEXTRACTOR_H
