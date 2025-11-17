#include "gifextractor.h"
#include <QMovie>
#include <QDebug>
#include <QApplication>

GifExtractor::GifExtractor(QObject *parent) : QObject(parent)
{
}

void GifExtractor::addFrame(const QImage &image)
{
  if (!image.isNull()) {
      m_frames.append(QPixmap::fromImage(image));
    }
}

QList<QPixmap> GifExtractor::extractFrames(const QString &filePath)
{
  m_frames.clear();

  QMovie movie(filePath);

  if (!movie.isValid()) {
      qWarning() << "Erreur: Le fichier n'est pas un GIF valide ou n'existe pas:" << filePath;
      return m_frames;
    }

  // TODO: dispatch images on a single pixmap and fill the ist with coordinates only
  movie.setCacheMode(QMovie::CacheAll);
  QObject::connect(&movie, &QMovie::frameChanged, [this, &movie]() {
    this->addFrame(movie.currentImage());
  });
  movie.start();
  while (movie.state() == QMovie::Running) {
      QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
      if (m_frames.size() >= movie.frameCount()) {
          movie.stop();
          break;
        }
    }

  emit extractionFinished(m_frames.size());
  return m_frames;
}
