#include "extractor/jsonextractor.h"
#include "src/extractor/jsonExtractordialog.h"
#include "ui_jsonExtractordialog.h"
#include "extractor/export.h"
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
  jsonExtractorDialog* dialog = new jsonExtractorDialog(in, projectName);
  dialog->exec();

  ExportOptions opts = dialog->getOpts();

  QJsonArray framesArray; // JSON array to store metadata for each frame.

  QList<QString> animationsNames = dialog->selectedAnimations();

  for (const QString &anim : animationsNames) {
      int totalFrames = in->m_animationsData[anim].frameIndices.count();

      for (int frame = 0; frame < totalFrames; ++frame) {

          int boxID = in->m_animationsData[anim].frameIndices[frame];
          // --- Build JSON Metadata (Coordinates on the NEW Atlas) ---
          // Record the actual position and size of the drawn frame for the JSON file.
          QJsonObject frameData;
          frameData["filename"] = QString("frame_%1").arg(frame, 4, 10, QChar('0'));

          QJsonObject frameRect;
          frameRect["x"] = in->m_frames[boxID].rect().x();
          frameRect["y"] = in->m_frames[boxID].rect().y();
          frameRect["w"] = in->m_frames[boxID].rect().width();
          frameRect["h"] = in->m_frames[boxID].rect().height();

          frameData["frame"] = frameRect;

          // Simulate common Texture Packer metadata fields.frameData["rotated"] = false;
          frameData["trimmed"] = false;
          frameData["spriteSourceSize"] = frameRect;

          framesArray.append(frameData);
        }
      QString jsonFilePath = QDir(basePath).filePath(projectName + "-" + anim + ".json");
      QString pngFilePath = QDir(basePath).filePath(projectName + ".png");

             // a) Save PNG Atlas
      if (!in->m_atlas.save(pngFilePath, "PNG")) {
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
          {"w", in->m_atlas.width()},
          {"h", in->m_atlas.height()}
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
    }
  return true; // Export completed successfully.
}
