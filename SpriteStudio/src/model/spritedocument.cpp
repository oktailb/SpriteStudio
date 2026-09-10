#include "model/spritedocument.h"
#include <QFileInfo>
#include <QPainter>
#include <algorithm>

SpriteDocument::SpriteDocument(QObject *parent)
    : QObject(parent)
{
}

void SpriteDocument::clear()
{
    m_atlas = QImage();
    m_frames.clear();
    m_boxes.clear();
    m_animations.clear();
    m_filePath.clear();
    m_maxFrameWidth = 0;
    m_maxFrameHeight = 0;

    emit documentReset();
}

QString SpriteDocument::projectName() const
{
    if (m_filePath.isEmpty()) {
        return QStringLiteral("untitled");
    }
    return QFileInfo(m_filePath).completeBaseName();
}

void SpriteDocument::setAtlas(const QImage &image)
{
    m_atlas = image;
    emit atlasChanged();
}

QPixmap SpriteDocument::frame(int index) const
{
    if (index >= 0 && index < m_frames.size()) {
        return m_frames.at(index);
    }
    return QPixmap();
}

void SpriteDocument::setFrames(const QList<QPixmap> &frames, const QList<SpriteBox> &boxes)
{
    m_frames = frames;
    m_boxes = boxes;
    recalculateMaxFrameDimensions();
    emit framesChanged();
}

void SpriteDocument::addFrame(const QPixmap &pixmap, const SpriteBox &box)
{
    if (!pixmap.isNull()) {
        m_frames.append(pixmap);
        m_boxes.append(box);
        if (pixmap.width() > m_maxFrameWidth) m_maxFrameWidth = pixmap.width();
        if (pixmap.height() > m_maxFrameHeight) m_maxFrameHeight = pixmap.height();
        emit framesChanged();
    }
}

void SpriteDocument::insertFrame(int index, const QPixmap &pixmap, const SpriteBox &box)
{
    if (index < 0 || index > m_frames.size() || pixmap.isNull()) return;

    m_frames.insert(index, pixmap);
    m_boxes.insert(index, box);

    // Shift frame indices in animations that are >= index
    for (auto it = m_animations.begin(); it != m_animations.end(); ++it) {
        QList<int> &indices = it.value().frameIndices;
        for (int i = 0; i < indices.size(); ++i) {
            if (indices[i] >= index) {
                indices[i]++;
            }
        }
    }

    recalculateMaxFrameDimensions();
    emit framesChanged();
    emit animationsChanged();
}

void SpriteDocument::removeFrame(int index)
{
    if (index < 0 || index >= m_frames.size()) return;

    m_frames.removeAt(index);
    if (index < m_boxes.size()) {
        m_boxes.removeAt(index);
    }

    // Update animations: remove referencing frames and shift indices down
    for (auto it = m_animations.begin(); it != m_animations.end(); ++it) {
        QList<int> updated;
        for (int frameIdx : it.value().frameIndices) {
            if (frameIdx == index) {
                continue; // Supprimé
            } else if (frameIdx > index) {
                updated.append(frameIdx - 1);
            } else {
                updated.append(frameIdx);
            }
        }
        it.value().frameIndices = updated;
    }

    recalculateMaxFrameDimensions();
    emit framesChanged();
    emit animationsChanged();
}

void SpriteDocument::removeFrames(const QList<int> &indices)
{
    if (indices.isEmpty()) return;

    QList<int> sortedIndices = indices;
    std::sort(sortedIndices.begin(), sortedIndices.end());
    sortedIndices.erase(std::unique(sortedIndices.begin(), sortedIndices.end()), sortedIndices.end());

    for (int i = sortedIndices.size() - 1; i >= 0; --i) {
        int idx = sortedIndices[i];
        if (idx >= 0 && idx < m_frames.size()) {
            m_frames.removeAt(idx);
            if (idx < m_boxes.size()) {
                m_boxes.removeAt(idx);
            }
        }
    }

    // Update animations
    for (auto it = m_animations.begin(); it != m_animations.end(); ++it) {
        QList<int> updated;
        for (int frameIdx : it.value().frameIndices) {
            if (sortedIndices.contains(frameIdx)) {
                continue;
            }
            int shift = 0;
            for (int removedIdx : sortedIndices) {
                if (removedIdx < frameIdx) {
                    shift++;
                }
            }
            updated.append(frameIdx - shift);
        }
        it.value().frameIndices = updated;
    }

    recalculateMaxFrameDimensions();
    emit framesChanged();
    emit animationsChanged();
}

void SpriteDocument::reorderFrames(const QList<int> &newOrder)
{
    if (newOrder.size() != m_frames.size()) return;

    QList<QPixmap> reorderedFrames;
    QList<SpriteBox> reorderedBoxes;

    for (int idx : newOrder) {
        if (idx >= 0 && idx < m_frames.size()) {
            reorderedFrames.append(m_frames[idx]);
            reorderedBoxes.append(idx < m_boxes.size() ? m_boxes[idx] : SpriteBox());
        }
    }

    m_frames = reorderedFrames;
    m_boxes = reorderedBoxes;

    // Create a mapping from old index -> new index
    QMap<int, int> oldToNew;
    for (int newPos = 0; newPos < newOrder.size(); ++newPos) {
        oldToNew[newOrder[newPos]] = newPos;
    }

    for (auto it = m_animations.begin(); it != m_animations.end(); ++it) {
        QList<int> updated;
        for (int oldIdx : it.value().frameIndices) {
            if (oldToNew.contains(oldIdx)) {
                updated.append(oldToNew[oldIdx]);
            }
        }
        it.value().frameIndices = updated;
    }

    emit framesChanged();
    emit animationsChanged();
}

void SpriteDocument::mergeFrames(int sourceIndex, int targetIndex)
{
    if (sourceIndex < 0 || sourceIndex >= m_frames.size() ||
        targetIndex < 0 || targetIndex >= m_frames.size() ||
        sourceIndex == targetIndex) {
        return;
    }

    SpriteBox srcBox = m_boxes.value(sourceIndex);
    SpriteBox tgtBox = m_boxes.value(targetIndex);

    QRect unitedRect = srcBox.rect.united(tgtBox.rect);
    QPixmap mergedPixmap;

    if (!m_atlas.isNull() && unitedRect.isValid()) {
        mergedPixmap = QPixmap::fromImage(m_atlas).copy(unitedRect);
    } else {
        // Fallback: draw both pixmaps side by side or overlay
        QSize combinedSize = m_frames[targetIndex].size().expandedTo(m_frames[sourceIndex].size());
        QImage composite(combinedSize, QImage::Format_ARGB32_Premultiplied);
        composite.fill(Qt::transparent);
        QPainter p(&composite);
        p.drawPixmap(0, 0, m_frames[targetIndex]);
        p.drawPixmap(0, 0, m_frames[sourceIndex]);
        p.end();
        mergedPixmap = QPixmap::fromImage(composite);
    }

    SpriteBox newBox;
    newBox.rect = unitedRect;
    newBox.selected = srcBox.selected || tgtBox.selected;
    newBox.index = tgtBox.index;

    m_boxes[targetIndex] = newBox;
    m_frames[targetIndex] = mergedPixmap;

    removeFrame(sourceIndex);
}

SpriteBox SpriteDocument::box(int index) const
{
    if (index >= 0 && index < m_boxes.size()) {
        return m_boxes.at(index);
    }
    return SpriteBox();
}

void SpriteDocument::setBox(int index, const SpriteBox &box)
{
    if (index >= 0 && index < m_boxes.size()) {
        m_boxes[index] = box;
        emit frameUpdated(index);
    }
}

void SpriteDocument::setBoxSelection(int index, bool selected)
{
    if (index >= 0 && index < m_boxes.size()) {
        m_boxes[index].selected = selected;
    }
}

void SpriteDocument::clearBoxSelections()
{
    for (int i = 0; i < m_boxes.size(); ++i) {
        m_boxes[i].selected = false;
    }
}

QList<int> SpriteDocument::selectedFrameIndices() const
{
    QList<int> res;
    for (int i = 0; i < m_boxes.size(); ++i) {
        if (m_boxes[i].selected) {
            res.append(i);
        }
    }
    return res;
}

SpriteAnimation SpriteDocument::animation(const QString &name) const
{
    return m_animations.value(name);
}

void SpriteDocument::setAnimation(const QString &name, const QList<int> &frameIndices, int fps, bool loop)
{
    SpriteAnimation anim;
    anim.name = name;
    anim.frameIndices = frameIndices;
    anim.fps = fps;
    anim.loop = loop;
    m_animations[name] = anim;

    emit animationsChanged();
}

void SpriteDocument::removeAnimation(const QString &name)
{
    if (m_animations.remove(name) > 0) {
        emit animationsChanged();
    }
}

void SpriteDocument::renameAnimation(const QString &oldName, const QString &newName)
{
    if (m_animations.contains(oldName) && !newName.isEmpty()) {
        SpriteAnimation anim = m_animations.take(oldName);
        anim.name = newName;
        m_animations[newName] = anim;
        emit animationsChanged();
    }
}

void SpriteDocument::reverseAnimationFrames(const QString &name)
{
    if (m_animations.contains(name)) {
        std::reverse(m_animations[name].frameIndices.begin(), m_animations[name].frameIndices.end());
        emit animationsChanged();
    }
}

void SpriteDocument::clearAtlasAreas(const QList<int> &frameIndices)
{
    if (m_atlas.isNull() || frameIndices.isEmpty()) return;

    QPainter painter(&m_atlas);
    painter.setCompositionMode(QPainter::CompositionMode_Clear);

    for (int idx : frameIndices) {
        if (idx >= 0 && idx < m_boxes.size()) {
            painter.fillRect(m_boxes[idx].rect, Qt::transparent);
        }
    }
    painter.end();
    emit atlasChanged();
}

void SpriteDocument::recalculateMaxFrameDimensions()
{
    m_maxFrameWidth = 0;
    m_maxFrameHeight = 0;
    for (const QPixmap &pix : m_frames) {
        if (pix.width() > m_maxFrameWidth) m_maxFrameWidth = pix.width();
        if (pix.height() > m_maxFrameHeight) m_maxFrameHeight = pix.height();
    }
}
