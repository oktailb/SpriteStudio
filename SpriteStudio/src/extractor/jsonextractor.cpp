#include "extractor/jsonextractor.h"
#include "extractor/jsonExtractordialog.h"
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

JsonExtractor::JsonExtractor(QLabel *statusBar, QProgressBar *progressBar, QObject *parent)
    : Extractor(statusBar, progressBar, parent)
{
}

QStringList findFilesGlob(const QString &path, const QString &fîlter)
{
  QStringList res;
  QDir dir(path);
  QStringList name_filters;
  name_filters << fîlter;
  QFileInfoList fil = dir.entryInfoList(name_filters, QDir::NoDotAndDotDot|QDir::AllDirs|QDir::Files);
  for (int i = 0; i < fil.size(); i++)
    {
      QFileInfo fi = fil.at(i);
      if (fi.isFile())
        res.push_back(fi.fileName());
    }
  return res;
}

bool getJsonRoot(const QString &filePath, QJsonObject & dest) {
  // 1. Charger et parser le fichier JSON
  QFile jsonFile(filePath);
  if (!jsonFile.open(QIODevice::ReadOnly)) {
      qWarning() << "Impossible d'ouvrir le fichier JSON:" << filePath
                  << "Erreur:" << jsonFile.errorString();
      return false;
    }

  QByteArray jsonData = jsonFile.readAll();
  jsonFile.close();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
      qWarning() << "Erreur de parsing JSON:" << parseError.errorString()
      << "à la position:" << parseError.offset;
      return false;
    }

  if (!doc.isObject()) {
      qWarning() << "Le document JSON n'est pas un objet";
      return false;
    }
  dest = doc.object();

  return true;
}

QList<QPixmap> JsonExtractor::extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance)
{
  Q_UNUSED(alphaThreshold);
  Q_UNUSED(verticalTolerance);

  m_frames.clear();
  m_atlas_index.clear();
  m_animationsData.clear();
  m_filePath = filePath;

  QFile jsonFile(m_filePath);
  QJsonObject root;
  bool ok = getJsonRoot(m_filePath, root);
  if (! ok) return m_frames;

         // 2. Extraire les informations du meta
  QString imageFileName;
  QSize atlasSize;

  if (root.contains("meta") && root["meta"].isObject()) {
      QJsonObject meta = root["meta"].toObject();

      if (meta.contains("image") && meta["image"].isString()) {
          imageFileName = meta["image"].toString();
        }

      if (meta.contains("size") && meta["size"].isObject()) {
          QJsonObject sizeObj = meta["size"].toObject();
          if (sizeObj.contains("w") && sizeObj["w"].isDouble() &&
              sizeObj.contains("h") && sizeObj["h"].isDouble()) {
              atlasSize = QSize(sizeObj["w"].toInt(), sizeObj["h"].toInt());
            }
        }
    }

         // 3. Construire le chemin vers l'image PNG
  QFileInfo jsonFileInfo(filePath);
  QString imageFilePath;

  if (!imageFileName.isEmpty()) {
      // Chercher le PNG dans le même répertoire que le JSON
      imageFilePath = jsonFileInfo.dir().filePath(imageFileName);

             // Si non trouvé, essayer avec le même nom de base que le JSON
      if (!QFile::exists(imageFilePath)) {
          QString baseName = jsonFileInfo.completeBaseName();
          imageFilePath = jsonFileInfo.dir().filePath(baseName + ".png");
        }
    } else {
      // Pas de nom d'image dans le JSON, utiliser le même nom que le JSON
      imageFilePath = jsonFileInfo.dir().filePath(jsonFileInfo.completeBaseName() + ".png");
    }

         // 4. Charger l'image atlas
  if (!QFile::exists(imageFilePath)) {
      qWarning() << "Fichier image non trouvé:" << imageFilePath;
      return m_frames;
    }

  m_atlas = QImage(imageFilePath);
  if (m_atlas.isNull()) {
      qWarning() << "Impossible de charger l'image atlas:" << imageFilePath;
      return m_frames;
    }

  QFileInfo  fileInfo(imageFilePath);
  QStringList friendAnimations = findFilesGlob(fileInfo.absolutePath(), fileInfo.baseName() + "-*.json");
  QMap<QString, QList<int>> animationFrames;

  for(auto currentAnimation : friendAnimations) {
      qDebug() << currentAnimation;
      QJsonObject currentRoot;
      bool ok = getJsonRoot(fileInfo.absolutePath() + QDir::separator() + currentAnimation, currentRoot);

             // 5. Extraire les frames selon le format
      bool hasFrameTags = false;

      if (currentRoot.contains("frames")) {
          // Format 1: TexturePacker (dictionnaire)
          if (currentRoot["frames"].isObject()) {
              QJsonObject framesObj = currentRoot["frames"].toObject();
              extractFromTexturePackerFormat(framesObj, animationFrames);
            }
          // Format 2: Aseprite/tableau
          else if (currentRoot["frames"].isArray()) {
              QJsonArray framesArray = currentRoot["frames"].toArray();
              extractFromArrayFormat(framesArray, animationFrames);
            }
        }

             // 6. Extraire les animations (frameTags)
      if (currentRoot.contains("meta") && currentRoot["meta"].isObject()) {
          QJsonObject meta = currentRoot["meta"].toObject();
          if (meta.contains("frameTags") && meta["frameTags"].isArray()) {
              hasFrameTags = true;
              extractAnimationsFromFrameTags(meta["frameTags"].toArray(), animationFrames, m_frames.count());
            }
        }

             // 7. Si pas d'animations définies, créer une animation par défaut
      if (animationFrames.isEmpty() && !m_frames.isEmpty()) {
          QString defaultAnimName = jsonFileInfo.completeBaseName();
          QList<int> allFrames;
          for (int i = 0; i < m_frames.size(); ++i) {
              allFrames.append(i);
            }
          animationFrames[defaultAnimName] = allFrames;
          m_animationsData[defaultAnimName].frameIndices = allFrames;
          m_animationsData[defaultAnimName].fps = 60; // FPS par défaut
        } else {
          for (auto animation : animationFrames.keys()) {
              animationFrames[animation] = animationFrames[animation];
              m_animationsData[animation].frameIndices = animationFrames[animation];
              m_animationsData[animation].fps = 6; // how to do ?
            }
        }
    }

         // 8. Émettre le signal de fin d'extraction
  emit extractionFinished(m_frames.size());

         // 9. Mettre à jour la status bar
  if (m_statusBar) {
      QString message = tr("_import_success") + " " +
                        QString::number(m_frames.size()) + " " + tr("_frames");
      if (!animationFrames.isEmpty()) {
          message += ", " + QString::number(animationFrames.size()) + " " + tr("_animations");
        }
      m_statusBar->setText(message);
    }

  return m_frames;
}

void JsonExtractor::extractFromTexturePackerFormat(const QJsonObject& framesObj,
                                               QMap<QString, QList<int>>& animationFrames)
{
  int frameIndex = 0;
  for (QString animation : animationFrames.keys())
    for (int id : animationFrames[animation])
      frameIndex++;

  m_progressBar->setValue(0);
  for (auto it = framesObj.begin(); it != framesObj.end(); ++it) {
      QString frameName = it.key();
      QJsonValue frameValue = it.value();

      if (!frameValue.isObject()) {
          qWarning() << "Format de frame invalide pour:" << frameName;
          continue;
        }

      QJsonObject frameObj = frameValue.toObject();

             // Extraire les coordonnées
      if (!frameObj.contains("frame") || !frameObj["frame"].isObject()) {
          qWarning() << "Frame sans coordonnées:" << frameName;
          continue;
        }

      QJsonObject frameRect = frameObj["frame"].toObject();

      int x = frameRect.value("x").toInt();
      int y = frameRect.value("y").toInt();
      int w = frameRect.value("w").toInt();
      int h = frameRect.value("h").toInt();

             // Vérifier les limites
      if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
          x + w > m_atlas.width() || y + h > m_atlas.height()) {
          qWarning() << "Coordonnées de frame invalides:" << frameName
                      << "x:" << x << "y:" << y << "w:" << w << "h:" << h;
          continue;
        }

             // Extraire la frame de l'atlas
      QPixmap frame = QPixmap::fromImage(m_atlas.copy(x, y, w, h));
      if (frame.isNull()) {
          qWarning() << "Impossible d'extraire la frame:" << frameName;
          continue;
        }

             // Ajouter à la liste
      m_frames.append(frame);

             // Créer la boîte de délimitation
      Extractor::Box box;
      box.rect = {x, y, w, h};
      box.selected = false;
      box.index = frameIndex;
      m_atlas_index.append(box);

             // Analyser le nom de frame pour détecter l'animation
             // Format attendu: "animation_0000"
      QString animName = extractAnimationName(frameName);
      if (!animName.isEmpty()) {
          animationFrames[animName].append(frameIndex);
        }
      m_progressBar->setValue(100 + frameIndex / framesObj.count());

      frameIndex++;

             // Mettre à jour la taille max
      if (w > m_maxFrameWidth) m_maxFrameWidth = w;
      if (h > m_maxFrameHeight) m_maxFrameHeight = h;
    }
  m_progressBar->setValue(100);
}

void JsonExtractor::extractFromArrayFormat(const QJsonArray& framesArray,
                                       QMap<QString, QList<int>>& animationFrames)
{
  for (int i = 0; i < framesArray.size(); ++i) {
      QJsonValue frameValue = framesArray[i];

      if (!frameValue.isObject()) {
          qWarning() << "Frame invalide à l'index:" << i;
          continue;
        }

      QJsonObject frameObj = frameValue.toObject();

             // Essayer différents formats de coordonnées
      QJsonObject frameRect;
      if (frameObj.contains("frame") && frameObj["frame"].isObject()) {
          frameRect = frameObj["frame"].toObject();
        } else if (frameObj.contains("x") && frameObj.contains("y") &&
               frameObj.contains("w") && frameObj.contains("h")) {
          frameRect = frameObj;
        } else {
          qWarning() << "Format de coordonnées inconnu pour la frame:" << i;
          continue;
        }

      int x = frameRect.value("x").toInt();
      int y = frameRect.value("y").toInt();
      int w = frameRect.value("w").toInt();
      int h = frameRect.value("h").toInt();

             // Vérifier les limites
      if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
          x + w > m_atlas.width() || y + h > m_atlas.height()) {
          qWarning() << "Coordonnées de frame invalides à l'index:" << i
                      << "x:" << x << "y:" << y << "w:" << w << "h:" << h;
          continue;
        }

             // Extraire la frame
      QPixmap frame = QPixmap::fromImage(m_atlas.copy(x, y, w, h));
      if (frame.isNull()) {
          qWarning() << "Impossible d'extraire la frame à l'index:" << i;
          continue;
        }

             // Ajouter à la liste
      m_frames.append(frame);

             // Créer la boîte
      Extractor::Box box;
      box.rect = {x, y, w, h};
      box.selected = false;
      box.index = i;
      m_atlas_index.append(box);

             // Extraire le nom d'animation depuis le filename si présent
      if (frameObj.contains("filename") && frameObj["filename"].isString()) {
          QString filename = frameObj["filename"].toString();
          QString animName = extractAnimationName(filename);
          if (!animName.isEmpty()) {
              animationFrames[animName].append(i);
            }
        }

             // Mettre à jour la taille max
      if (w > m_maxFrameWidth) m_maxFrameWidth = w;
      if (h > m_maxFrameHeight) m_maxFrameHeight = h;
    }
}

void JsonExtractor::extractAnimationsFromFrameTags(const QJsonArray& frameTagsArray,
                                               QMap<QString, QList<int>>& animationFrames,
                                               int shift)
{
  for (const QJsonValue& tagValue : frameTagsArray) {
      if (!tagValue.isObject()) continue;

      QJsonObject tagObj = tagValue.toObject();

      if (!tagObj.contains("name") || !tagObj["name"].isString() ||
          !tagObj.contains("from") || !tagObj["from"].isDouble() ||
          !tagObj.contains("to") || !tagObj["to"].isDouble()) {
          qWarning() << "Format de frameTag invalide";
          continue;
        }

      QString animName = tagObj["name"].toString();
      int from = tagObj["from"].toInt();
      int to = tagObj["to"].toInt();

             // Construire la liste des frames pour cette animation
      QList<int> frames;
      for (int i = from; i <= to; ++i) {
          if (i >= 0 && i < m_frames.size()) {
              frames.append(i + shift);
            }
        }

      if (!frames.isEmpty()) {
          animationFrames[animName] = frames;

                 // Extraire le FPS si présent
          int fps = 60; // Par défaut
          if (tagObj.contains("fps") && tagObj["fps"].isDouble()) {
              fps = tagObj["fps"].toInt();
            }

                 // Enregistrer dans m_animationsData
          m_animationsData[animName].frameIndices = frames;
          m_animationsData[animName].fps = fps;
        }
    }
}

QString JsonExtractor::extractAnimationName(const QString& frameName)
{
  // Formats supportés:
  // 1. "animation_0000" -> "animation"
  // 2. "frame_0000" -> "" (pas d'animation)
  // 3. "walk_001" -> "walk"

  QRegularExpression regex("^([a-zA-Z]+)_\\d+$");
  QRegularExpressionMatch match = regex.match(frameName);

  if (match.hasMatch()) {
      QString animName = match.captured(1);
      // Ignorer "frame" comme nom d'animation
      if (animName.toLower() != "frame") {
          return animName;
        }
    }

  return QString();
}

QList<QPixmap> JsonExtractor::extractFromPixmap(int alphaThreshold, int verticalTolerance)
{
  // Implementation of the pure virtual method.
  // JsonExtractor does not support re-extraction based on alpha/tolerance.
  Q_UNUSED(alphaThreshold);
  Q_UNUSED(verticalTolerance);

  return m_frames;
}

QJsonDocument *JsonExtractor::exportToTexturePacker(QString projectName,
                                      const ExportOptions &opts,
                                      const QString anim,
                                      const QString format,
                                      Extractor * in)
{
  QList<int> frameIndices = in->m_animationsData[anim].frameIndices;

  QJsonObject framesDict;

  m_progressBar->setValue(0);
  for (int i = 0; i < frameIndices.size(); ++i) {
      m_progressBar->setValue(100 * i / frameIndices.size());

      int frameIdx = frameIndices[i];

      if (frameIdx < 0 || frameIdx >= in->m_atlas_index.size()) {
          qWarning() << "Frame index out of bounds:" << frameIdx;
          continue;
        }

      const Extractor::Box& realBox = in->m_atlas_index[frameIdx];

      QJsonObject frameData;
      frameData["filename"] = QString("%1_%2").arg(anim).arg(i, 4, 10, QChar('0'));

      QJsonObject frameRect;
      frameRect["x"] = realBox.rect.x();
      frameRect["y"] = realBox.rect.y();
      frameRect["w"] = realBox.rect.width();
      frameRect["h"] = realBox.rect.height();

      frameData["frame"] = frameRect;
      frameData["rotated"] = opts.rotateSprites;
      frameData["trimmed"] = opts.trimSprites;

      frameData["spriteSourceSize"] = frameRect;

      QJsonObject sourceSize;
      sourceSize["w"] = realBox.rect.width();
      sourceSize["h"] = realBox.rect.height();
      frameData["sourceSize"] = sourceSize;

      QString frameKey = QString("%1_%2").arg(anim).arg(i, 4, 10, QChar('0'));
      framesDict[frameKey] = frameData;
    }
  m_progressBar->setValue(100);
  QJsonObject root;
  root["frames"] = framesDict;

  QJsonObject meta;
  meta["app"] = "Sprite Studio";
  meta["version"] = "1.0";
  meta["image"] = projectName + ".png";
  meta["format"] = format;
  meta["size"] = QJsonObject{
    {"w", in->m_atlas.width()},
    {"h", in->m_atlas.height()}
  };
  meta["scale"] = "1";

  if (opts.embedAnimations) {
      QJsonArray frameTags;
      QJsonObject animTag;
      animTag["name"] = anim;
      animTag["from"] = 0;
      animTag["to"] = frameIndices.size() - 1;
      animTag["direction"] = "forward";
      frameTags.append(animTag);
      meta["frameTags"] = frameTags;
    }

  root["meta"] = meta;

  return new QJsonDocument(root);
}

bool JsonExtractor::exportFrames(const QString &basePath, const QString &projectName, Extractor *in)
{
  jsonExtractorDialog* dialog = new jsonExtractorDialog(in, projectName);
  dialog->exec();

  ExportOptions opts = dialog->getOpts();

  QList<QString> animationsNames = dialog->selectedAnimations();

  for (const QString &anim : animationsNames) {

      QJsonDocument * doc = nullptr;

      switch (dialog->selectedFormat())
        {
        case Format::FORMAT_TEXTUREPACKER_JSON: { doc = exportToTexturePacker(projectName, opts, anim, dialog->imageFormatAsString(), in); break; }
        case Format::FORMAT_PHASER_JSON: { doc = exportToTexturePacker(projectName, opts, anim, dialog->imageFormatAsString(), in); break; }
        case Format::FORMAT_ASEPRITE_JSON: { doc = exportToTexturePacker(projectName, opts, anim, dialog->imageFormatAsString(), in); break; }
        default: {
            m_statusBar->setText(tr("_selected_format_error"));
            return false;
          }
        }


      QString jsonFilePath = QDir(basePath).filePath(projectName + "-" + anim + ".json");

      QFile jsonFile(jsonFilePath);
      if (jsonFile.open(QIODevice::WriteOnly)) {
          // Write JSON data to file in an indented (human-readable) format.
          jsonFile.write(doc->toJson(QJsonDocument::Indented));
          jsonFile.close();
          m_statusBar->setText(tr("_export_success") + tr("_export_atlas_success") + basePath);
        } else {
          m_statusBar->setText(tr("_write_error") + tr("_json_permissions"));
          delete doc;
          return false;
        }
      delete doc;
    }

  if (dialog->replaceAtlas()) {
      m_atlas = m_atlas.convertToFormat(dialog->imageFormat());
      QString pngFilePath = QDir(basePath).filePath(projectName + ".png");

      if (!in->m_atlas.save(pngFilePath, "PNG")) {
          m_statusBar->setText(tr("_write_error") + ": " + tr("_png_permissions"));
          return false;
        }
    }
  m_statusBar->setText(tr("_success"));
  return true; // Export completed successfully.
}
