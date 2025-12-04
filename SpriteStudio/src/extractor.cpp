#include "extractor/extractor.h"
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
    return 60; // Modern value on industry
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

    std::reverse(frameIndices.begin(), frameIndices.end());
}

void Extractor::removeFrame(int index)
{
    if (index < 0 || index >= m_frames.size()) return;

    m_frames.removeAt(index);
    m_atlas_index.removeAt(index);

    for (auto it = m_animationsData.begin(); it != m_animationsData.end(); ++it) {
        QList<int> &frameIndices = it.value().frameIndices;

        frameIndices.removeAll(index);

        for (int &frameIndex : frameIndices) {
            if (frameIndex > index) {
                frameIndex--;
            }
        }

        QSet<int> uniqueIndices;
        for (int frameIndex : frameIndices) {
            uniqueIndices.insert(frameIndex);
        }
        frameIndices = uniqueIndices.values();

        std::sort(frameIndices.begin(), frameIndices.end());
    }
}

void Extractor::removeFrames(const QList<int> &indices)
{
    if (indices.isEmpty()) return;

    QList<int> sortedIndices = indices;
    std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());

    for (int index : sortedIndices) {
        if (index >= 0 && index < m_frames.size()) {
            m_frames.removeAt(index);
            m_atlas_index.removeAt(index);
        }
    }

    for (auto it = m_animationsData.begin(); it != m_animationsData.end(); ++it) {
        QList<int> &frameIndices = it.value().frameIndices;
        QList<int> updatedIndices;

        for (int frameIndex : frameIndices) {
            int newIndex = frameIndex;

            for (int removedIndex : sortedIndices) {
                if (frameIndex == removedIndex) {
                    newIndex = -1;
                    break;
                } else if (frameIndex > removedIndex) {
                    newIndex--;
                }
            }

            if (newIndex >= 0) {
                updatedIndices.append(newIndex);
            }
        }

        QSet<int> uniqueIndices;
        for (int index : updatedIndices) {
            uniqueIndices.insert(index);
        }
        frameIndices = uniqueIndices.values();
        std::sort(frameIndices.begin(), frameIndices.end());
    }
}

void Extractor::clearAtlasAreas(const QList<int> &indices)
{
    if (m_atlas.isNull() || indices.isEmpty()) return;

    QImage atlasImage = m_atlas;
    if (atlasImage.format() != QImage::Format_ARGB32) {
        atlasImage = atlasImage.convertToFormat(QImage::Format_ARGB32);
    }

    for (int index : indices) {
        if (index >= 0 && index < m_atlas_index.size()) {
            const Box &box = m_atlas_index.at(index);

            for (int y = box.rect.y(); y < box.rect.bottom(); ++y) {
                for (int x = box.rect.x(); x < box.rect.right(); ++x) {
                    if (x < atlasImage.width() && y < atlasImage.height()) {
                        atlasImage.setPixel(x, y, qRgba(0, 0, 0, 0)); // Transparent
                    }
                }
            }
        }
    }
    m_atlas = atlasImage;
}

ExportOptions Extractor::opts() const
{
    return m_opts;
}

double Extractor::overlapThreshold() const
{
  return m_overlapThreshold;
}

void Extractor::setOverlapThreshold(double newOverlapThreshold)
{
  m_overlapThreshold = newOverlapThreshold;
}

bool Extractor::smartCropEnabled() const
{
    return m_smartCropEnabled;
}

void Extractor::setSmartCropEnabled(bool newSmartCropEnabled)
{
    m_smartCropEnabled = newSmartCropEnabled;
}
