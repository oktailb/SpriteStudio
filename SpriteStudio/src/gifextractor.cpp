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
  // Constructor. Initialization is handled by the Extractor base class.
}

QList<QPixmap> GifExtractor::extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance)
{
  // Store the file path in a member variable (m_filePath) for use in the helper function.
  m_filePath = filePath;

  // Clear the internal state containers before starting a new extraction.
  m_frames.clear();
  m_atlas_index.clear();
  m_atlas = QPixmap();

  // Delegate the actual extraction logic to the helper function.
  // Alpha and Vertical Tolerance are typically ignored for GIFs.
  return extractFromPixmap(alphaThreshold, verticalTolerance);
}

QList<QPixmap> GifExtractor::extractFromPixmap(int alphaThreshold, int verticalTolerance)
{
  // Mark parameters as unused since they are required by the Extractor interface but not used here.
  Q_UNUSED(alphaThreshold);
  Q_UNUSED(verticalTolerance);

  QList<QImage> extractedImages;
  QMovie movie(m_filePath); // QMovie is Qt's class for handling GIF files.

  if (!movie.isValid()) {
      // Log an error if the GIF file is invalid or doesn't exist.
      qWarning() << tr("_error") << tr("_invalid_gif") << m_filePath;
      return m_frames;
    }

  int nb_frames = movie.frameCount();
  if (nb_frames <= 0) {
      // Log an error if the GIF reports zero frames.
      qWarning() << tr("_gif_no_frames");
      return m_frames;
    }

  // Cache all frames in memory for reliable extraction and fast sequential access.
  movie.setCacheMode(QMovie::CacheAll);

  // Set up a connection to capture each frame as QMovie advances.
  QObject::connect(&movie, &QMovie::frameChanged,
                    [&movie, &extractedImages, this](int frameNumber)
                    {
                      QImage currentImage = movie.currentImage();
                      if (currentImage.isNull())
                        return;

                      // Store the image in the local list for atlas assembly
                      extractedImages.append(currentImage);

                      // Store the QPixmap in the Extractor's internal list (m_frames)
                      // for the frames list view in the main window.
                      this->addFrame(QPixmap::fromImage(currentImage));
                      qDebug() << tr("_extracted_frames") << ": " << frameNumber;
                    });

  // Start playback and jump to the first frame to initiate frame extraction via the signal.
  movie.start();
  movie.jumpToFrame(0);

  // This loop forces the QMovie to process all frames synchronously.
  while (movie.state() == QMovie::Running) {
      // Process events to allow QMovie to emit the frameChanged signal and update internally.
      // Exclude user input events to keep the UI from responding during background processing.
      QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

      // Break condition: stop when all expected frames have been extracted.
      if (extractedImages.size() >= nb_frames) {
          movie.stop();
          break;
        }
    }

  if (extractedImages.isEmpty()) {
      qWarning() << "Extraction des frames échouée.";
      return m_frames;
    }

  // --- Atlas Assembly Logic ---

  // Get the base width and height of all frames. GIFs have a fixed size.
  int w = movie.frameRect().width();
  int h = movie.frameRect().height();
  nb_frames = extractedImages.size();

  // Calculate optimal grid layout (as close to square as possible) for the atlas image.
  int nb_cols = (int)std::floor(std::sqrt(nb_frames));
  if (nb_cols == 0) nb_cols = 1;
  int nb_lines = (int)std::ceil((double)nb_frames / nb_cols);

  // Create the final QImage for the atlas with calculated dimensions and transparent background.
  QImage atlasImage(w * nb_cols, h * nb_lines, QImage::Format_ARGB32_Premultiplied);
  atlasImage.fill(Qt::transparent);

  QPainter painter(&atlasImage);

  if (!painter.isActive()) {
      qWarning() << "Échec critique: QPainter ne peut pas démarrer même en contexte synchrone.";
      return m_frames;
    }

  // Reset frame size tracking and draw each frame onto the atlas.
  m_maxFrameWidth = 0;
  m_maxFrameHeight = 0;
  for (int i = 0; i < nb_frames; ++i) {
      const QImage &currentImage = extractedImages.at(i);

      // Calculate grid position (line and column)
      int line = i / nb_cols;
      int col = i % nb_cols;
      int x = col * w;
      int y = line * h;

      // Draw the frame at its calculated position
      painter.drawImage(x, y, currentImage);

      // Record the bounding box coordinates (Box) for the metadata file (m_atlas_index).
      m_atlas_index.push_back({x, y, w, h});

      // Update maximum frame dimensions (w and h are the max since all GIF frames are the same size)
      if (w > m_maxFrameWidth) {
          m_maxFrameWidth = w;
        }
      if (h > m_maxFrameHeight) {
          m_maxFrameHeight = h;
        }
    }

  // Finalize the atlas Pixmap for display in the main view.
  m_atlas = QPixmap::fromImage(atlasImage);

  // Signal the main window that the extraction is complete (e.g., to populate the frame list).
  emit extractionFinished(m_frames.size());

  return m_frames;
}

bool GifExtractor::exportFrames(const QString &basePath, const QString &projectName, Extractor *in)
{
  // Implementation of the pure virtual method.
  // GifExtractor does not support re-extraction based on alpha/tolerance (it's for sequential frames).
  Q_UNUSED(basePath);
  Q_UNUSED(projectName);
  Q_UNUSED(in);

  qDebug() << "TODO: exportFrames GIF";
  return false;
}
