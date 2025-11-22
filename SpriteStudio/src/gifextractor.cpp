#include "gifextractor.h"
#include <QMovie>
#include <QDebug>
#include <QApplication>
#include <QPainter>
#include <cmath>
#include <QImage>
#include <QCoreApplication>

GifExtractor::GifExtractor(QObject *parent) : Extractor(parent)
{
}

QList<QPixmap> GifExtractor::extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance)
{
  m_filePath = filePath;
  m_frames.clear();
  m_atlas_index.clear();
  m_atlas = QPixmap();

  return extractFromPixmap(alphaThreshold, verticalTolerance);
}

QList<QPixmap> GifExtractor::extractFromPixmap(int alphaThreshold, int verticalTolerance)
{
  Q_UNUSED(alphaThreshold);
  Q_UNUSED(verticalTolerance);
  QList<QImage> extractedImages;
  QMovie movie(m_filePath);

  if (!movie.isValid()) {
      qWarning() << tr("Erreur: Le fichier n'est pas un GIF valide ou n'existe pas:") << m_filePath;
      return m_frames;
    }

  int nb_frames = movie.frameCount();
  if (nb_frames <= 0) {
      qWarning() << tr("Erreur: Le GIF ne contient aucune frame.");
      return m_frames;
    }

  movie.setCacheMode(QMovie::CacheAll);

  QObject::connect(&movie, &QMovie::frameChanged,
                    [&movie, &extractedImages, this](int frameNumber)
                    {
                      QImage currentImage = movie.currentImage();
                      if (currentImage.isNull())
                        return;
                      extractedImages.append(currentImage);
                      this->addFrame(QPixmap::fromImage(currentImage));
                      qDebug() << "Frame extraite (QImage) :" << frameNumber;
                    });

  movie.start();
  movie.jumpToFrame(0);

  while (movie.state() == QMovie::Running) {
      QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

      if (extractedImages.size() >= nb_frames) {
          movie.stop();
          break;
        }
    }

  if (extractedImages.isEmpty()) {
      qWarning() << "Extraction des frames échouée.";
      return m_frames;
    }

  int w = movie.frameRect().width();
  int h = movie.frameRect().height();
  nb_frames = extractedImages.size();

  int nb_cols = (int)std::floor(std::sqrt(nb_frames));
  if (nb_cols == 0) nb_cols = 1;
  int nb_lines = (int)std::ceil((double)nb_frames / nb_cols);

  QImage atlasImage(w * nb_cols, h * nb_lines, QImage::Format_ARGB32_Premultiplied);
  atlasImage.fill(Qt::transparent);

  QPainter painter(&atlasImage);

  if (!painter.isActive()) {
      qWarning() << "Échec critique: QPainter ne peut pas démarrer même en contexte synchrone.";
      return m_frames;
    }

  maxFrameWidth = 0;
  maxFrameHeight = 0;
  for (int i = 0; i < nb_frames; ++i) {
      const QImage &currentImage = extractedImages.at(i);

      int line = i / nb_cols;
      int col = i % nb_cols;
      int x = col * w;
      int y = line * h;
      painter.drawImage(x, y, currentImage);
      m_atlas_index.push_back({x, y, w, h});
      if (w > maxFrameWidth) {
          maxFrameWidth = w;
        }
      if (h > maxFrameHeight) {
          maxFrameHeight = h;
        }
    }

  m_atlas = QPixmap::fromImage(atlasImage);

  qDebug() << "Extraction et Assemblage de l'Atlas terminés. Frames extraites:" << m_atlas_index.size();
  emit extractionFinished(m_frames.size());

  return m_frames;
}

bool GifExtractor::exportFrames(const QString &basePath, const QString &projectName, Extractor *in)
{
  Q_UNUSED(basePath);
  Q_UNUSED(projectName);
  Q_UNUSED(in);

  return false;
}
