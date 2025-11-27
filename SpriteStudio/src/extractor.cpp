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
