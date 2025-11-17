#include "spriteextractor.h"
#include <QMovie>
#include <QDebug>
#include <QApplication>

SpriteExtractor::SpriteExtractor(QObject *parent) : QObject(parent)
{
}

void SpriteExtractor::addFrame(const QPixmap &pixmap)
{
  if (!pixmap.isNull()) {
      m_frames.append(pixmap);
    }
}

QList<QPixmap> SpriteExtractor::extractFrames(const QString &filePath)
{
  m_frames.clear();

  QPixmap pixmap(filePath);

  // TODO: detect each sprite on the map
  this->addFrame(pixmap);

  emit extractionFinished(m_frames.size());
  return m_frames;
}
