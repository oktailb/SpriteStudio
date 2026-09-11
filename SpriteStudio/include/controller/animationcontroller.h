#ifndef ANIMATIONCONTROLLER_H
#define ANIMATIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QTreeWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

class AnimationPlayer;
class SpriteDocument;
class QUndoStack;

/**
 * @brief Controller managing animation playback, preview rendering, and animation list CRUD.
 */
class AnimationController : public QObject
{
    Q_OBJECT

public:
    explicit AnimationController(SpriteDocument *document,
                                 QUndoStack *undoStack = nullptr,
                                 AnimationPlayer *player = nullptr,
                                 QTreeWidget *treeWidget = nullptr,
                                 QGraphicsView *previewView = nullptr,
                                 QObject *parent = nullptr);
    ~AnimationController() override;

    // Playback control
    void play();
    void pause();
    void stop();
    void togglePlayPause();
    void stepForward();
    void stepBackward();
    void seek(int sequenceIndex);
    bool isPlaying() const;

    int fps() const;
    void setFps(int fps);

    QString currentAnimationName() const { return m_currentAnimationName; }
    int currentSequenceIndex() const;
    int currentGlobalFrameIndex() const;

    // Animation CRUD & Selection
    void selectAnimation(const QString &name);
    void createAnimation(const QString &name, const QList<int> &frameIndices, int fps = 12);
    void createAnimationFromSelection(const QList<int> &selectedIndices);
    void removeAnimation(const QString &name);
    void removeAnimations(const QStringList &names);
    void removeSelectedAnimation();
    void reverseAnimationOrder();

    // Transient "current" selection animation
    void updateCurrentAnimation(const QList<int> &selectedIndices);
    void removeCurrentAnimation();
    bool hasCurrentAnimation() const;

    // View & UI sync
    void syncAnimationList();
    void updatePreview();
    void attachTreeWidget(QTreeWidget *treeWidget);
    void attachPreviewView(QGraphicsView *previewView);

    AnimationPlayer* player() const { return m_player; }
    QGraphicsScene* previewScene() const { return m_previewScene; }

signals:
    void playbackStateChanged(bool isPlaying);
    void frameChanged(int sequenceIndex, int globalIndex);
    void fpsChanged(int fps);
    void currentAnimationChanged(const QString &name);
    void animationListChanged();
    void statusMessage(const QString &message);
    void framesSelectedInAnimation(const QList<int> &frameIndices);

private slots:
    void onPlayerFrameChanged(int seqIndex, int globalIndex);
    void onPlayerPlaybackStateChanged(bool playing);
    void onTreeItemSelectionChanged();
    void onTreeItemClicked(QTreeWidgetItem *item, int column);

private:
    void renderCurrentFrame(int globalFrameIndex);

    SpriteDocument      *m_document = nullptr;
    QUndoStack          *m_undoStack = nullptr;
    AnimationPlayer     *m_player = nullptr;
    bool                 m_ownsPlayer = false;
    QTreeWidget         *m_treeWidget = nullptr;
    QGraphicsView       *m_previewView = nullptr;
    QGraphicsScene      *m_previewScene = nullptr;
    QGraphicsPixmapItem *m_previewPixmapItem = nullptr;
    QString              m_currentAnimationName;
};

#endif // ANIMATIONCONTROLLER_H
