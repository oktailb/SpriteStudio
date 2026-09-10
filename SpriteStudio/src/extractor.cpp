#include "extractor/extractor.h"
#include "model/spritedocument.h"
#include <QDebug>
#include <QFileInfo>
#include <algorithm>

Extractor::Extractor(QObject *parent)
    : QObject(parent)
    , m_maxFrameWidth(0)
    , m_maxFrameHeight(0)
    , m_smartCropEnabled(true)
    , m_overlapThreshold(0.1)
    , m_cropStrategy(SeparateStrategy)
{
}

void Extractor::setProgress(int percentage)
{
    m_progress = percentage;
    emit progress(percentage);
}

void Extractor::setStatusMessage(const QString &message)
{
    m_statusMessage = message;
    emit statusMessage(message);
}

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
        frameIndices = updatedIndices;
    }
}

void Extractor::clearAtlasAreas(const QList<int> &indices)
{
    if (m_atlas.isNull() || indices.isEmpty()) return;

    QImage atlasImage = m_atlas;
    if (atlasImage.format() != QImage::Format_ARGB32) {
        atlasImage = atlasImage.convertToFormat(QImage::Format_ARGB32);
    }

    QPainter painter(&atlasImage);
    painter.setCompositionMode(QPainter::CompositionMode_Clear);

    for (int index : indices) {
        if (index >= 0 && index < m_atlas_index.size()) {
            const Box &box = m_atlas_index.at(index);
            painter.fillRect(box.rect, Qt::transparent);
        }
    }
    painter.end();
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

void Extractor::syncToDocument(SpriteDocument &doc) const
{
    doc.setFilePath(m_filePath);
    doc.setAtlas(m_atlas);
    QList<SpriteBox> sboxes;
    sboxes.reserve(m_atlas_index.size());
    for (const auto &b : m_atlas_index) {
        SpriteBox sb;
        sb.rect = b.rect;
        sb.selected = b.selected;
        sb.index = b.index;
        sb.groupId = b.groupId;
        sb.overlappingBoxes = b.overlappingBoxes;
        sboxes.append(sb);
    }
    doc.setFrames(m_frames, sboxes);
    for (auto it = m_animationsData.begin(); it != m_animationsData.end(); ++it) {
        doc.setAnimation(it.key(), it.value().frameIndices, it.value().fps, true);
    }
}

void Extractor::syncFromDocument(const SpriteDocument &doc)
{
    m_filePath = doc.filePath();
    m_atlas = doc.atlas();
    m_frames = doc.frames();
    m_atlas_index.clear();
    m_atlas_index.reserve(doc.boxes().size());
    for (const auto &sb : doc.boxes()) {
        Box b;
        b.rect = sb.rect;
        b.selected = sb.selected;
        b.index = sb.index;
        b.groupId = sb.groupId;
        b.overlappingBoxes = sb.overlappingBoxes;
        m_atlas_index.append(b);
    }
    m_maxFrameWidth = doc.maxFrameWidth();
    m_maxFrameHeight = doc.maxFrameHeight();
    m_animationsData.clear();
    for (auto it = doc.animations().begin(); it != doc.animations().end(); ++it) {
        AnimationData ad;
        ad.frameIndices = it.value().frameIndices;
        ad.fps = it.value().fps;
        m_animationsData[it.key()] = ad;
    }
}

bool Extractor::extract(const QString &filePath, SpriteDocument &doc, QString *errorMsg)
{
    m_filePath = filePath;
    m_frames = extractFrames(filePath, 20, 5);
    if (m_frames.isEmpty()) {
        if (errorMsg) *errorMsg = tr("No frames could be extracted from: %1").arg(filePath);
        return false;
    }
    syncToDocument(doc);
    return true;
}

bool Extractor::exportDocument(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, QString *errorMsg)
{
    m_opts = options;
    syncFromDocument(doc);
    QFileInfo fi(filePath);
    QString basePath = fi.absolutePath();
    QString projectName = fi.completeBaseName();
    bool ok = exportFrames(basePath, projectName, this);
    if (!ok && errorMsg) {
        *errorMsg = tr("Export failed for %1").arg(filePath);
    }
    return ok;
}
