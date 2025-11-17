#ifndef GIFEXTRACTOR_H
#define GIFEXTRACTOR_H

#include <QObject>
#include <QPixmap>
#include <QList>
#include <QString>

class GifExtractor : public QObject
{
  Q_OBJECT
public:
  explicit GifExtractor(QObject *parent = nullptr);

  QList<QPixmap> extractFrames(const QString &filePath);

private:
  void addFrame(const QImage &image);

signals:
  void extractionFinished(int frameCount);

private:
  QList<QPixmap> m_frames;
};

#endif // GIFEXTRACTOR_H
