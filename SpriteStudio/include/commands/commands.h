#ifndef COMMANDS_H
#define COMMANDS_H

#include <QUndoCommand>
#include <QPixmap>
#include <QList>
#include <QMap>
#include "model/spritedocument.h"

/**
 * @brief Command to delete a set of frames from the document with full undo capability.
 */
class DeleteFramesCommand : public QUndoCommand
{
public:
    DeleteFramesCommand(SpriteDocument *doc, const QList<int> &indices, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    struct FrameBackup {
        int       originalIndex;
        QPixmap   pixmap;
        SpriteBox box;
    };

    SpriteDocument*                 m_doc;
    QList<int>                      m_indicesToDelete;
    QList<FrameBackup>              m_deletedFrames;
    QMap<QString, SpriteAnimation>  m_animationsBackup;
};

/**
 * @brief Command to merge one frame onto another with full undo capability.
 */
class MergeFramesCommand : public QUndoCommand
{
public:
    MergeFramesCommand(SpriteDocument *doc, int sourceIndex, int targetIndex, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument*                 m_doc;
    int                             m_sourceIndex;
    int                             m_targetIndex;
    QPixmap                         m_sourcePixmap;
    SpriteBox                       m_sourceBox;
    QPixmap                         m_targetOriginalPixmap;
    SpriteBox                       m_targetOriginalBox;
    QMap<QString, SpriteAnimation>  m_animationsBackup;
};

/**
 * @brief Command to create a new animation sequence.
 */
class CreateAnimationCommand : public QUndoCommand
{
public:
    CreateAnimationCommand(SpriteDocument *doc, const QString &name, const QList<int> &frameIndices, int fps, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument* m_doc;
    QString         m_name;
    QList<int>      m_frameIndices;
    int             m_fps;
};

/**
 * @brief Command to delete an animation sequence with undo support.
 */
class DeleteAnimationCommand : public QUndoCommand
{
public:
    DeleteAnimationCommand(SpriteDocument *doc, const QString &name, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument* m_doc;
    QString         m_name;
    SpriteAnimation m_backup;
};

/**
 * @brief Command to reverse the order of frames in an animation.
 */
class ReverseAnimationCommand : public QUndoCommand
{
public:
    ReverseAnimationCommand(SpriteDocument *doc, const QString &animName, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument* m_doc;
    QString         m_animName;
};

#endif // COMMANDS_H
