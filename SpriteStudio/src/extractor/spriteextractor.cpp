#include "extractor/spriteextractor.h"
#include <QDebug>
#include <QStack>

SpriteExtractor::SpriteExtractor(QLabel *statusBar, QProgressBar *progressBar, QObject *parent)
    : Extractor(statusBar, progressBar, parent)
{
}

QList<QPixmap> SpriteExtractor::extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance)
{
  QPixmap atlasPixmap(filePath);
  if (atlasPixmap.isNull()) {
      qWarning() << "Erreur: Impossible de charger l'image:" << filePath;
      return m_frames;
    }
  m_atlas = atlasPixmap.toImage();
  m_filePath = filePath;

  return extractFromPixmap(alphaThreshold, verticalTolerance);
}

QList<QPixmap> SpriteExtractor::extractFromPixmap(int alphaThreshold, int verticalTolerance)
{
  m_frames.clear();
  m_atlas_index.clear();

  m_maxFrameWidth = 0;
  m_maxFrameHeight = 0;

  QImage image = m_atlas;

  if (image.format() != QImage::Format_ARGB32 && image.hasAlphaChannel()) {
      image = image.convertToFormat(QImage::Format_ARGB32);
    } else if (!image.hasAlphaChannel()) {
      // Force Alpha layer if BMP/JPG
      image = image.convertToFormat(QImage::Format_ARGB32);
    }

  int w = image.width();
  int h = image.height();

  const int ALPHA_THRESHOLD = alphaThreshold;
  QVector<QVector<bool>> visited(h, QVector<bool>(w, false));
  QList<Extractor::Box> detectedBoxes;

  for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
          if (!visited[y][x] && qAlpha(image.pixel(x, y)) > ALPHA_THRESHOLD) {
              int minX = w;
              int minY = h;
              int maxX = -1;
              int maxY = -1;

              QStack<QPoint> stack;
              stack.push(QPoint(x, y));
              visited[y][x] = true;

              while (!stack.isEmpty()) {
                  QPoint current = stack.pop();
                  int curX = current.x();
                  int curY = current.y();

                  minX = qMin(minX, curX);
                  minY = qMin(minY, curY);
                  maxX = qMax(maxX, curX);
                  maxY = qMax(maxY, curY);

                  int dx[] = {0, 0, 1, -1};
                  int dy[] = {1, -1, 0, 0};

                  for (int i = 0; i < 4; ++i) {
                      int nextX = curX + dx[i];
                      int nextY = curY + dy[i];

                      if (nextX >= 0 && nextX < w && nextY >= 0 && nextY < h) {
                          if (!visited[nextY][nextX] && qAlpha(image.pixel(nextX, nextY)) > ALPHA_THRESHOLD) {
                              visited[nextY][nextX] = true;
                              stack.push(QPoint(nextX, nextY));
                            }
                        }
                    }
                }

              if (minX <= maxX && minY <= maxY) {
                  int spriteW = maxX - minX + 1;
                  int spriteH = maxY - minY + 1;

                  Box box;
                  box.rect = {minX, minY, spriteW, spriteH};
                  box.index  = 0;
                  box.selected = false;
                  detectedBoxes.push_back(box);
                }
            }
        }
    }

  std::sort( detectedBoxes.begin(),
             detectedBoxes.end(),
             [&verticalTolerance](const Extractor::Box& a, const Extractor::Box& b) {
               const int VERTICAL_TOLERANCE = verticalTolerance;
               if (std::abs(a.rect.y() - b.rect.y()) <= VERTICAL_TOLERANCE) {
                   return a.rect.x() < b.rect.x();
                 }
               return a.rect.y() < b.rect.y();
             });

  if (detectedBoxes.size() > 0) {
      QList<QRect> rects;
      for (const auto& box : detectedBoxes) {
          rects.append(box.rect);
        }

      QList<bool> isMaster(rects.size(), true);

      for (int i = 0; i < rects.size(); ++i) {
          for (int j = 0; j < rects.size(); ++j) {
              if (i == j) continue;

              if (rects[j].contains(rects[i])) {
                  isMaster[i] = false;
                  break;
                }
            }
        }

      m_atlas_index.clear();
      for (int i = 0; i < rects.size(); ++i) {
          if (isMaster[i]) {
              QRect rect = rects[i];
              Box box;
              box.rect = rect;
              box.index = i;
              box.selected = false;
              m_atlas_index.push_back(box);
            }
        }
    }

  if (m_smartCropEnabled && m_atlas_index.size() > 1) {
      detectAndResolveOverlaps(m_atlas_index, image, alphaThreshold, m_overlapThreshold);
  }

  for (const auto& box : m_atlas_index) {
      QPixmap spriteFrame = QPixmap::fromImage(m_atlas).copy(box.rect);
      this->addFrame(spriteFrame);
      if (spriteFrame.width() > m_maxFrameWidth) {
          m_maxFrameWidth = spriteFrame.width();
        }
      if (spriteFrame.height() > m_maxFrameHeight) {
          m_maxFrameHeight = spriteFrame.height();
        }
    }

  emit extractionFinished(m_frames.size());
  return m_frames;
}

void SpriteExtractor::detectAndResolveOverlaps(QList<Box>& boxes,
                                               const QImage& image,
                                               int alphaThreshold,
                                               double overlapThreshold)
{
    // Étape 1: Détecter les chevauchements significatifs
    for (int i = 0; i < boxes.size(); ++i) {
        boxes[i].overlappingBoxes.clear(); // Nettoyer d'abord
        boxes[i].groupId = -1;
    }

    for (int i = 0; i < boxes.size(); ++i) {
        for (int j = i + 1; j < boxes.size(); ++j) {
            double overlapRatio = calculateOverlapRatio(boxes[i], boxes[j]);
            if (overlapRatio > overlapThreshold) {
                boxes[i].overlappingBoxes.append(j);
                boxes[j].overlappingBoxes.append(i);
            }
        }
    }

    // Étape 2: Grouper
    int currentGroupId = 0;
    QVector<int> groupAssigned(boxes.size(), -1);

    for (int i = 0; i < boxes.size(); ++i) {
        if (groupAssigned[i] == -1) {
            assignGroup(boxes, groupAssigned, i, currentGroupId++);
        }
    }

    // Étape 3: Résolution des groupes
    resolveOverlappingGroups(boxes, groupAssigned, image, alphaThreshold);
}

void SpriteExtractor::resolveOverlappingGroups(QList<Box>& boxes,
                                               const QVector<int>& groupAssigned,
                                               const QImage& image,
                                               int alphaThreshold)
{
    QMap<int, QList<int>> groups;

    // Regrouper par ID de groupe
    for (int i = 0; i < boxes.size(); ++i) {
        groups[groupAssigned[i]].append(i);
    }

    QList<Box> finalBoxes;

    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const QList<int>& groupIndices = it.value();

        if (groupIndices.size() == 1) {
            // Cas simple: pas de chevauchement
            finalBoxes.append(boxes[groupIndices.first()]);
        } else {
            // Cas complexe: plusieurs sprites qui se chevauchent
            QList<Box> separatedBoxes = separateOverlappingSprites(
                boxes, groupIndices, image, alphaThreshold);
            finalBoxes.append(separatedBoxes);
        }
    }

    boxes = finalBoxes;
}

QList<SpriteExtractor::Box> SpriteExtractor::separateOverlappingSprites(
    const QList<Box>& boxes,
    const QList<int>& groupIndices,
    const QImage& image,
    int alphaThreshold)
{
    QList<Box> result;

    if (groupIndices.isEmpty()) {
        return result;
    }

    // Calculer les limites du groupe
    QRect groupBounds = calculateGroupBounds(boxes, groupIndices);

    // Créer un masque binaire du groupe
    QImage mask = QImage(groupBounds.size(), QImage::Format_ARGB32);
    mask.fill(Qt::transparent);

    // Remplir le masque avec les pixels opaques du groupe
    for (int index : groupIndices) {
        const Box& box = boxes[index];
        QRect sourceRect(box.rect);

        // Pour chaque pixel dans la boîte
        for (int y = 0; y < box.rect.height(); ++y) {
            int globalY = box.rect.y() + y;
            int maskY = globalY - groupBounds.y();

            for (int x = 0; x < box.rect.width(); ++x) {
                int globalX = box.rect.x() + x;
                int maskX = globalX - groupBounds.x();

                // Vérifier les limites
                if (maskX >= 0 && maskX < mask.width() &&
                    maskY >= 0 && maskY < mask.height()) {

                    QColor pixel = image.pixelColor(globalX, globalY);
                    if (pixel.alpha() > alphaThreshold) {
                        // Marquer comme opaque dans le masque
                        mask.setPixelColor(maskX, maskY, QColor(255, 255, 255, 255));
                    }
                }
            }
        }
    }

    // Extraire les composantes connexes du masque
    QList<Box> separatedComponents = extractConnectedComponentsFromMask(
        mask, groupBounds, alphaThreshold);

    // Filtrer les composantes trop petites (bruit)
    for (const Box& component : separatedComponents) {
        if (component.rect.width() * component.rect.height()> 4) { // Au moins 4 pixels
            result.append(component);
        }
    }

    return result;
}

void SpriteExtractor::assignGroup(QList<Box>& boxes, QVector<int>& groupAssigned,
                                  int currentIndex, int groupId)
{
    if (groupAssigned[currentIndex] != -1) return;

    groupAssigned[currentIndex] = groupId;
    boxes[currentIndex].groupId = groupId;

    // Propagation récursive aux boîtes qui chevauchent
    for (int overlappingIndex : boxes[currentIndex].overlappingBoxes) {
        if (groupAssigned[overlappingIndex] == -1) {
            assignGroup(boxes, groupAssigned, overlappingIndex, groupId);
        }
    }
}

QList<SpriteExtractor::Box> SpriteExtractor::extractConnectedComponentsFromMask(
    const QImage& mask, const QRect& groupBounds, int alphaThreshold)
{
    QList<Box> components;
    int w = mask.width();
    int h = mask.height();

    QVector<QVector<bool>> visited(h, QVector<bool>(w, false));

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!visited[y][x] && mask.pixelColor(x, y).alpha() > alphaThreshold) {
                // Nouvelle composante détectée
                int minX = w, minY = h, maxX = -1, maxY = -1;
                int pixelCount = 0;

                QStack<QPoint> stack;
                stack.push(QPoint(x, y));
                visited[y][x] = true;

                while (!stack.isEmpty()) {
                    QPoint p = stack.pop();
                    int curX = p.x();
                    int curY = p.y();

                    // Mettre à jour les limites
                    minX = qMin(minX, curX);
                    minY = qMin(minY, curY);
                    maxX = qMax(maxX, curX);
                    maxY = qMax(maxY, curY);
                    pixelCount++;

                    // Voisinage 4-connexe
                    const int dx[] = {0, 1, 0, -1};
                    const int dy[] = {1, 0, -1, 0};

                    for (int i = 0; i < 4; ++i) {
                        int nx = curX + dx[i];
                        int ny = curY + dy[i];

                        if (nx >= 0 && nx < w && ny >= 0 && ny < h &&
                            !visited[ny][nx] && mask.pixelColor(nx, ny).alpha() > alphaThreshold) {
                            visited[ny][nx] = true;
                            stack.push(QPoint(nx, ny));
                        }
                    }
                }

                // Créer la boîte seulement si suffisamment de pixels
                if (pixelCount > 10) { // Seuil minimal pour éviter le bruit
                    Box component;
                    component.rect.setX(groupBounds.x() + minX);
                    component.rect.setY(groupBounds.y() + minY);
                    component.rect.setWidth(maxX - minX + 1);
                    component.rect.setHeight(maxY - minY + 1);
                    component.selected = false;
                    component.index = components.size();
                    component.groupId = -1; // Réinitialiser pour le nouveau groupe

                    components.append(component);
                }
            }
        }
    }

    return components;
}

double SpriteExtractor::calculateOverlapRatio(const Box& a, const Box& b)
{
    QRect rectA(a.rect);
    QRect rectB(b.rect);

    QRect intersection = rectA.intersected(rectB);
    if (!intersection.isValid()) return 0.0;

    double areaA = rectA.width() * rectA.height();
    double areaB = rectB.width() * rectB.height();
    double areaIntersection = intersection.width() * intersection.height();

    // Retourner le ratio de chevauchement le plus significatif
    return qMax(areaIntersection / areaA, areaIntersection / areaB);
}

QImage SpriteExtractor::extractGroupMask(const QImage& sourceImage, const QRect& groupBounds,
                                         const QList<Box>& boxes, const QList<int>& groupIndices,
                                         int alphaThreshold)
{
    QImage mask(groupBounds.size(), QImage::Format_ARGB32);
    mask.fill(Qt::transparent);

    QPainter painter(&mask);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Dessiner toutes les zones opaques du groupe
    for (int index : groupIndices) {
        const Box& box = boxes[index];
        QRect sourceRect(box.rect);
        QRect destRect(box.rect);

        // Copier uniquement les pixels au-dessus du seuil alpha
        for (int y = 0; y < box.rect.height(); ++y) {
            for (int x = 0; x < box.rect.width(); ++x) {
                QColor pixel = sourceImage.pixelColor(box.rect.x() + x, box.rect.y() + y);
                if (pixel.alpha() > alphaThreshold) {
                    mask.setPixelColor(destRect.x() + x, destRect.y() + y, Qt::white);
                }
            }
        }
    }

    painter.end();
    return mask;
}

QRect SpriteExtractor::calculateGroupBounds(const QList<Box>& boxes, const QList<int>& indices)
{
    if (indices.isEmpty()) return QRect();

    QRect bounds = boxes[indices.first()].rect;

    for (int i = 1; i < indices.size(); ++i) {
        const Box& box = boxes[indices[i]];
        bounds = bounds.united(box.rect);
    }

    return bounds;
}

bool SpriteExtractor::exportFrames(const QString &basePath, const QString &projectName, Extractor *in)
{
  Q_UNUSED(basePath);
  Q_UNUSED(projectName);
  Q_UNUSED(in);

  return false;
}
