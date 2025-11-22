#include "jsonextractor.h"
#include <QDebug>
#include <QImage>
#include <QStack>
#include <QPoint>
#include <QRect>
#include <qdir.h>
#include <qjsonarray.h>
#include <qjsonobject.h>

JsonExtractor::JsonExtractor(QObject *parent) : Extractor(parent)
{
}

QList<QPixmap> JsonExtractor::extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance)
{
  Q_UNUSED(alphaThreshold);
  Q_UNUSED(verticalTolerance);

  m_frames.clear();
  m_atlas_index.clear();
  m_filePath = filePath;
  return m_frames;
}

QList<QPixmap> JsonExtractor::extractFromPixmap(int alphaThreshold, int verticalTolerance)
{
  Q_UNUSED(alphaThreshold);
  Q_UNUSED(verticalTolerance);

  return m_frames;
}

bool JsonExtractor::exportFrames(const QString &basePath, const QString &projectName, Extractor *in)
{

  int totalFrames = in->m_frames.size();
  if (totalFrames == 0) return false;

  int w = in->m_maxFrameWidth;
  int h = in->m_maxFrameHeight;
  int nb_cols = (int)std::floor(std::sqrt(totalFrames));
  if (nb_cols == 0) nb_cols = 1;
  int nb_lines = (int)std::ceil((double)totalFrames / nb_cols);

  QImage atlasImage(w * nb_cols, h * nb_lines, QImage::Format_ARGB32_Premultiplied);
  atlasImage.fill(Qt::transparent);
  QPainter painter(&atlasImage);

  if (!painter.isActive()) {
      qDebug() << tr("Erreur Critique"), tr("Impossible de démarrer le peintre pour l'atlas.");
      return false;
    }

  QJsonArray framesArray; // JSON array pour les métadonnées

  for (int i = 0; i < totalFrames; ++i) {
      const QPixmap &currentPixmap = in->m_frames.at(i);
      QImage currentImage = currentPixmap.toImage();

      int line = i / nb_cols;
      int col = i % nb_cols;
      int x = col * w;
      int y = line * h;

             // Centrer la frame dans sa case (basé sur la logique d'animation)
      int x_offset = x + (w - currentImage.width()) / 2;
      int y_offset = y + (h - currentImage.height()) / 2;

      painter.drawImage(QPoint(x_offset, y_offset), currentImage);

             // --- 2. Construire les Métadonnées JSON (Coordonnées sur l'Atlas) ---
             // Chaque frame a maintenant une position fixe sur l'atlas

      QJsonObject frameData;
      frameData["filename"] = QString("frame_%1").arg(i, 4, 10, QChar('0'));

      QJsonObject frameRect;
      frameRect["x"] = x_offset;
      frameRect["y"] = y_offset;
      frameRect["w"] = currentImage.width();
      frameRect["h"] = currentImage.height();

      frameData["frame"] = frameRect;

             // Simuler des métadonnées de texture packer (ajoutez plus si nécessaire)
      frameData["rotated"] = false;
      frameData["trimmed"] = false;
      frameData["spriteSourceSize"] = frameRect; // Simplifié pour l'exemple

      framesArray.append(frameData);
    }

  painter.end();

         // --- 3. Sauvegarde des Fichiers ---

         // Nom des fichiers de sortie
  QString pngFilePath = QDir(basePath).filePath(projectName + ".png");
  QString jsonFilePath = QDir(basePath).filePath(projectName + ".json");

         // a) Sauvegarde PNG
  if (!atlasImage.save(pngFilePath, "PNG")) {
      qDebug() << tr("Erreur d'écriture"), tr("Impossible d'écrire le fichier PNG. Vérifiez les permissions.");
      return false;
    }

         // b) Sauvegarde JSON
  QJsonObject root;
  root["frames"] = framesArray;

  QJsonObject meta;
  meta["image"] = projectName + ".png";
  meta["size"] = QJsonObject({
      {"w", atlasImage.width()},
      {"h", atlasImage.height()}
  });
  meta["format"] = "RGBA8888";
  root["meta"] = meta;

  QJsonDocument doc(root);

  QFile jsonFile(jsonFilePath);
  if (jsonFile.open(QIODevice::WriteOnly)) {
      jsonFile.write(doc.toJson(QJsonDocument::Indented)); // Format indenté et lisible
      jsonFile.close();

      qDebug() << tr("Exportation réussie"), tr("L'atlas et les métadonnées ont été exportés avec succès : %1").arg(basePath);
    } else {
      qDebug() << tr("Erreur d'écriture"), tr("Impossible d'écrire le fichier JSON. Vérifiez les permissions.");
      return false;
    }
  return true;
}
