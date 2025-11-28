#include "extractor.h"
#include <QDebug>

void Extractor::setAnimation(const QString &name, const QList<int> &frameIndices, int fps)
{
    AnimationData animData;
    animData.frameIndices = frameIndices;
    animData.fps = fps;
    m_animationsData[name] = animData;
}

void Extractor::removeAnimation(const QString &name)
{
    m_animationsData.remove(name);
}

QList<int> Extractor::getAnimationFrames(const QString &name) const
{
    if (m_animationsData.contains(name)) {
        return m_animationsData[name].frameIndices;
    }
    return QList<int>();
}

int Extractor::getAnimationFps(const QString &name) const
{
    if (m_animationsData.contains(name)) {
        return m_animationsData[name].fps;
    }
    return 60; // Valeur par défaut
}

QStringList Extractor::getAnimationNames() const
{
    return m_animationsData.keys();
}

void Extractor::reorderFrames(const QList<int> &newOrder)
{
    if (newOrder.size() != m_frames.size()) {
        qWarning() << "Le nouvel ordre ne correspond pas au nombre de frames";
        return;
    }

    // Réorganiser les frames et les boxes
    QList<QPixmap> newFrames;
    QList<Box> newBoxes;

    for (int newIndex : newOrder) {
        if (newIndex >= 0 && newIndex < m_frames.size()) {
            newFrames.append(m_frames[newIndex]);
            newBoxes.append(m_atlas_index[newIndex]);
        }
    }

    m_frames = newFrames;
    m_atlas_index = newBoxes;

    // Mettre à jour les animations
    for (auto &animData : m_animationsData) {
        QList<int> updatedIndices;
        for (int oldIndex : animData.frameIndices) {
            int newIndex = newOrder.indexOf(oldIndex);
            if (newIndex != -1) {
                updatedIndices.append(newIndex);
            }
        }
        animData.frameIndices = updatedIndices;
    }
}

void Extractor::reverseAnimationFrames(const QString &animationName)
{
    if (!m_animationsData.contains(animationName)) {
        qWarning() << "Animation" << animationName << "not found for reversal";
        return;
    }

    AnimationData &animData = m_animationsData[animationName];
    QList<int> &frameIndices = animData.frameIndices;

    // Inverser simplement l'ordre des indices
    std::reverse(frameIndices.begin(), frameIndices.end());

    qDebug() << "Animation" << animationName << "frames reversed. New order:" << frameIndices;
}

void Extractor::removeFrame(int index)
{
    if (index < 0 || index >= m_frames.size()) return;

    m_frames.removeAt(index);
    m_atlas_index.removeAt(index);

    // Mettre à jour les animations
    for (auto it = m_animationsData.begin(); it != m_animationsData.end(); ++it) {
        QList<int> &frameIndices = it.value().frameIndices;

        // Supprimer l'index de la frame
        frameIndices.removeAll(index);

        // Décrémenter les indices supérieurs
        for (int &frameIndex : frameIndices) {
            if (frameIndex > index) {
                frameIndex--;
            }
        }

        // Supprimer les doublons (méthode moderne)
        QSet<int> uniqueIndices;
        for (int frameIndex : frameIndices) {
            uniqueIndices.insert(frameIndex);
        }
        frameIndices = uniqueIndices.values();

        // Trier
        std::sort(frameIndices.begin(), frameIndices.end());
    }
}

void Extractor::removeFrames(const QList<int> &indices)
{
    if (indices.isEmpty()) return;

    // Trier par ordre décroissant pour suppression sécurisée
    QList<int> sortedIndices = indices;
    std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());

    // Supprimer les frames et boxes
    for (int index : sortedIndices) {
        if (index >= 0 && index < m_frames.size()) {
            m_frames.removeAt(index);
            m_atlas_index.removeAt(index);
        }
    }

    // Mettre à jour les animations
    for (auto it = m_animationsData.begin(); it != m_animationsData.end(); ++it) {
        QList<int> &frameIndices = it.value().frameIndices;
        QList<int> updatedIndices;

        for (int frameIndex : frameIndices) {
            int newIndex = frameIndex;

            // Ajuster les indices après suppression
            for (int removedIndex : sortedIndices) {
                if (frameIndex == removedIndex) {
                    // Frame supprimée, on l'ignore
                    newIndex = -1;
                    break;
                } else if (frameIndex > removedIndex) {
                    // Décrémenter l'index
                    newIndex--;
                }
            }

            if (newIndex >= 0) {
                updatedIndices.append(newIndex);
            }
        }

        // Supprimer les doublons et trier
        QSet<int> uniqueIndices;
        for (int index : updatedIndices) {
            uniqueIndices.insert(index);
        }
        frameIndices = uniqueIndices.values();
        std::sort(frameIndices.begin(), frameIndices.end());
    }

    qDebug() << "Removed" << indices.size() << "frames. Remaining:" << m_frames.size();
}

void Extractor::clearAtlasAreas(const QList<int> &indices)
{
    if (m_atlas.isNull() || indices.isEmpty()) return;

    QImage atlasImage = m_atlas.toImage();
    if (atlasImage.format() != QImage::Format_ARGB32) {
        atlasImage = atlasImage.convertToFormat(QImage::Format_ARGB32);
    }

    // Effacer chaque zone correspondant aux frames supprimées
    for (int index : indices) {
        if (index >= 0 && index < m_atlas_index.size()) {
            const Box &box = m_atlas_index.at(index);

            // Remplir la zone avec de la transparence
            for (int y = box.y; y < box.y + box.h; ++y) {
                for (int x = box.x; x < box.x + box.w; ++x) {
                    if (x < atlasImage.width() && y < atlasImage.height()) {
                        atlasImage.setPixel(x, y, qRgba(0, 0, 0, 0)); // Transparent
                    }
                }
            }
        }
    }

    // Mettre à jour l'atlas
    m_atlas = QPixmap::fromImage(atlasImage);

    qDebug() << "Cleared" << indices.size() << "areas from atlas";
}
