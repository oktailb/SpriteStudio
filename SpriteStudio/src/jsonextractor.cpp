#include "jsonextractor.h"
#include <QDebug>
#include <QImage>
#include <QStack>
#include <QPoint>
#include <QRect>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

JsonExtractor::JsonExtractor(QObject *parent) : Extractor(parent)
{
}

QList<QPixmap> JsonExtractor::extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance)
{
  // The JsonExtractor is primarily an EXPORTER, not an importer.
  // This method is a required implementation but serves as a no-op here.
  Q_UNUSED(alphaThreshold);
  Q_UNUSED(verticalTolerance);

  m_frames.clear();
  m_atlas_index.clear();
  m_filePath = filePath;
  return m_frames;
}

QList<QPixmap> JsonExtractor::extractFromPixmap(int alphaThreshold, int verticalTolerance)
{
  // Implementation of the pure virtual method.
  // JsonExtractor does not support re-extraction based on alpha/tolerance.
  Q_UNUSED(alphaThreshold);
  Q_UNUSED(verticalTolerance);

  return m_frames;
}

bool JsonExtractor::exportFrames(const QString &basePath, const QString &projectName, Extractor *in)
{
  // 'in' is the source extractor (e.g., SpriteExtractor) containing the frames and metadata.
  int totalFrames = in->m_frames.size();
  if (totalFrames == 0)
    return false;

  // Use the maximum width/height found during the initial extraction to define grid cell size.
  int w = in->m_maxFrameWidth;
  int h = in->m_maxFrameHeight;

  // Calculate the dimensions of the new atlas image grid (optimal square-like layout).
  int nb_cols = (int)std::floor(std::sqrt(totalFrames));
  if (nb_cols == 0) nb_cols = 1;
  int nb_lines = (int)std::ceil((double)totalFrames / nb_cols);

  // 1. ASSEMBLE NEW ATLAS IMAGE (PNG)

  // Create a new image buffer for the final combined atlas with calculated dimensions.
  QImage atlasImage(w * nb_cols, h * nb_lines, QImage::Format_ARGB32_Premultiplied);
  atlasImage.fill(Qt::transparent); // Ensure a fully transparent background.
  QPainter painter(&atlasImage); // Start drawing on the atlas image.

  if (!painter.isActive()) {
      qDebug() << tr("_critical_error") << ": " << tr("_painter_start");
      return false;
    }

  QJsonArray framesArray; // JSON array to store metadata for each frame.

  for (int i = 0; i < totalFrames; ++i) {
      const QPixmap &currentPixmap = in->m_frames.at(i);
      QImage currentImage = currentPixmap.toImage();

      // Calculate top-left position of the frame's cell in the grid.
      int line = i / nb_cols;
      int col = i % nb_cols;
      int x = col * w;
      int y = line * h;

      // Center the smaller frame within its fixed cell (w x h) for consistent alignment.
      int x_offset = x + (w - currentImage.width()) / 2;
      int y_offset = y + (h - currentImage.height()) / 2;

      // Draw the frame onto the atlas at its final coordinates.
      painter.drawImage(QPoint(x_offset, y_offset), currentImage);

      // --- 2. Build JSON Metadata (Coordinates on the NEW Atlas) ---
      // Record the actual position and size of the drawn frame for the JSON file.
      QJsonObject frameData;
      frameData["filename"] = QString("frame_%1").arg(i, 4, 10, QChar('0'));

      QJsonObject frameRect;
      frameRect["x"] = x_offset;
      frameRect["y"] = y_offset;
      frameRect["w"] = currentImage.width();
      frameRect["h"] = currentImage.height();

      frameData["frame"] = frameRect;

      // Simulate common Texture Packer metadata fields.frameData["rotated"] = false;
      frameData["trimmed"] = false;
      frameData["spriteSourceSize"] = frameRect;

      framesArray.append(frameData);
    }

  painter.end();// Stop drawing and finalize the QImage content.

  // --- 3. Save Files (PNG and JSON) ---

  // Define output file paths.
  QString pngFilePath = QDir(basePath).filePath(projectName + ".png");
  QString jsonFilePath = QDir(basePath).filePath(projectName + ".json");

  // a) Save PNG Atlas
  if (!atlasImage.save(pngFilePath, "PNG")) {
      qDebug() << tr("_write_error") << ": " <<  tr("_png_permissions");
      return false;
    }

  // b) Save JSON Metadata
  QJsonObject root;
  root["frames"] = framesArray;

  // Add metadata block for the entire atlas image.
  QJsonObject meta;
  meta["image"] = projectName + ".png"; // Reference to the atlas file
  meta["size"] = QJsonObject({
      {"w", atlasImage.width()},
      {"h", atlasImage.height()}
  });
  meta["format"] = "RGBA8888"; // Specify color format
  root["meta"] = meta;

  QJsonDocument doc(root); // Create the final JSON document.

  QFile jsonFile(jsonFilePath);
  if (jsonFile.open(QIODevice::WriteOnly)) {
      // Write JSON data to file in an indented (human-readable) format.
      jsonFile.write(doc.toJson(QJsonDocument::Indented));
      jsonFile.close();

      qDebug() << tr("_export_success") << tr("_export_atlas_success") << basePath;
    } else {
      qDebug() << tr("_write_error") << tr("_json_permissions");
      return false;
    }
  return true; // Export completed successfully.
}
