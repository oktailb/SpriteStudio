#include "commands/commands.h"
#include <algorithm>

// --- DeleteFramesCommand ---

DeleteFramesCommand::DeleteFramesCommand(SpriteDocument *doc, const QList<int> &indices, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_indicesToDelete(indices)
{
    setText(QObject::tr("Delete %n frame(s)", "", indices.size()));

    // Sort ascending for backup recording
    std::sort(m_indicesToDelete.begin(), m_indicesToDelete.end());
    m_indicesToDelete.erase(std::unique(m_indicesToDelete.begin(), m_indicesToDelete.end()), m_indicesToDelete.end());

    for (int idx : m_indicesToDelete) {
        if (idx >= 0 && idx < m_doc->frameCount()) {
            FrameBackup fb;
            fb.originalIndex = idx;
            fb.pixmap = m_doc->frame(idx);
            fb.box = m_doc->box(idx);
            m_deletedFrames.append(fb);
        }
    }

    m_animationsBackup = m_doc->animations();
}

void DeleteFramesCommand::redo()
{
    m_doc->removeFrames(m_indicesToDelete);
}

void DeleteFramesCommand::undo()
{
    // Reinsert deleted frames in ascending order
    for (const FrameBackup &fb : m_deletedFrames) {
        m_doc->insertFrame(fb.originalIndex, fb.pixmap, fb.box);
    }

    // Restore exact animations state
    for (auto it = m_animationsBackup.begin(); it != m_animationsBackup.end(); ++it) {
        m_doc->setAnimation(it.key(), it.value().frameIndices, it.value().fps, it.value().loop);
    }
}

// --- MergeFramesCommand ---

MergeFramesCommand::MergeFramesCommand(SpriteDocument *doc, int sourceIndex, int targetIndex, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_sourceIndex(sourceIndex)
    , m_targetIndex(targetIndex)
{
    setText(QObject::tr("Merge Frame %1 into %2").arg(sourceIndex + 1).arg(targetIndex + 1));

    m_sourcePixmap = m_doc->frame(sourceIndex);
    m_sourceBox = m_doc->box(sourceIndex);
    m_targetOriginalPixmap = m_doc->frame(targetIndex);
    m_targetOriginalBox = m_doc->box(targetIndex);
    m_animationsBackup = m_doc->animations();
}

void MergeFramesCommand::redo()
{
    m_doc->mergeFrames(m_sourceIndex, m_targetIndex);
}

void MergeFramesCommand::undo()
{
    // Restore target frame to its original pixmap and box
    int adjustedTarget = (m_sourceIndex < m_targetIndex) ? (m_targetIndex - 1) : m_targetIndex;
    if (adjustedTarget >= 0 && adjustedTarget < m_doc->frameCount()) {
        m_doc->removeFrame(adjustedTarget);
    }

    // Reinsert both original frames
    if (m_sourceIndex <= m_targetIndex) {
        m_doc->insertFrame(m_sourceIndex, m_sourcePixmap, m_sourceBox);
        m_doc->insertFrame(m_targetIndex, m_targetOriginalPixmap, m_targetOriginalBox);
    } else {
        m_doc->insertFrame(m_targetIndex, m_targetOriginalPixmap, m_targetOriginalBox);
        m_doc->insertFrame(m_sourceIndex, m_sourcePixmap, m_sourceBox);
    }

    // Restore animations
    for (auto it = m_animationsBackup.begin(); it != m_animationsBackup.end(); ++it) {
        m_doc->setAnimation(it.key(), it.value().frameIndices, it.value().fps, it.value().loop);
    }
}

// --- CreateAnimationCommand ---

CreateAnimationCommand::CreateAnimationCommand(SpriteDocument *doc, const QString &name, const QList<int> &frameIndices, int fps, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_name(name)
    , m_frameIndices(frameIndices)
    , m_fps(fps)
{
    setText(QObject::tr("Create Animation '%1'").arg(name));
}

void CreateAnimationCommand::redo()
{
    m_doc->setAnimation(m_name, m_frameIndices, m_fps, true);
}

void CreateAnimationCommand::undo()
{
    m_doc->removeAnimation(m_name);
}

// --- DeleteAnimationCommand ---

DeleteAnimationCommand::DeleteAnimationCommand(SpriteDocument *doc, const QString &name, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_name(name)
{
    setText(QObject::tr("Delete Animation '%1'").arg(name));
    m_backup = m_doc->animation(name);
}

void DeleteAnimationCommand::redo()
{
    m_doc->removeAnimation(m_name);
}

void DeleteAnimationCommand::undo()
{
    m_doc->setAnimation(m_backup.name, m_backup.frameIndices, m_backup.fps, m_backup.loop);
}

// --- ReverseAnimationCommand ---

ReverseAnimationCommand::ReverseAnimationCommand(SpriteDocument *doc, const QString &animName, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_animName(animName)
{
    setText(QObject::tr("Reverse Animation '%1'").arg(animName));
}

void ReverseAnimationCommand::redo()
{
    m_doc->reverseAnimationFrames(m_animName);
}

void ReverseAnimationCommand::undo()
{
    m_doc->reverseAnimationFrames(m_animName);
}

// ============================================================================
// ChangeBoxRectCommand
// ============================================================================
ChangeBoxRectCommand::ChangeBoxRectCommand(SpriteDocument *doc, int boxIndex, const QRect &oldRect, const QRect &newRect, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_index(boxIndex)
    , m_oldRect(oldRect)
    , m_newRect(newRect)
{
    setText(QObject::tr("Resize/Move Slice %1").arg(boxIndex + 1));
}

void ChangeBoxRectCommand::redo()
{
    m_doc->updateBoxRect(m_index, m_newRect);
}

void ChangeBoxRectCommand::undo()
{
    m_doc->updateBoxRect(m_index, m_oldRect);
}

// ============================================================================
// AddSliceCommand
// ============================================================================
AddSliceCommand::AddSliceCommand(SpriteDocument *doc, const QRect &rect, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_rect(rect)
    , m_createdIndex(-1)
{
    setText(QObject::tr("Add Slice"));
}

void AddSliceCommand::redo()
{
    if (m_createdIndex < 0) {
        m_createdIndex = m_doc->addSlice(m_rect);
    } else {
        QPixmap pm = QPixmap::fromImage(m_doc->atlas().copy(m_rect));
        SpriteBox box;
        box.rect = m_rect;
        box.index = m_createdIndex;
        box.selected = true;
        m_doc->insertFrame(m_createdIndex, pm, box);
    }
}

void AddSliceCommand::undo()
{
    if (m_createdIndex >= 0 && m_createdIndex < m_doc->frameCount()) {
        m_doc->removeFrame(m_createdIndex);
    }
}
