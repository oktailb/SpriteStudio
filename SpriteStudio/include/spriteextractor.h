#ifndef SPRITEEXTRACTOR_H
#define SPRITEEXTRACTOR_H

#include <QObject>
#include <QPixmap>
#include <QList>
#include <QString>

class SpriteExtractor : public QObject
{
  Q_OBJECT
public:
  explicit SpriteExtractor(QObject *parent = nullptr);

  QList<QPixmap> extractFrames(const QString &filePath);

private:
  void addFrame(const QPixmap &image);

signals:
  void extractionFinished(int frameCount);

private:
  QList<QPixmap> m_frames;
};

#endif // SPRITEEXTRACTOR_H
