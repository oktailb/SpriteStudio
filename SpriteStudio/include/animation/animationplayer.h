#ifndef ANIMATIONPLAYER_H
#define ANIMATIONPLAYER_H

#include <QObject>
#include <QTimer>
#include <QList>

/**
 * @brief Autonomous animation controller managing playback timing and frame progression.
 */
class AnimationPlayer : public QObject
{
    Q_OBJECT

public:
    explicit AnimationPlayer(QObject *parent = nullptr);
    ~AnimationPlayer() override = default;

    // Sequence setup
    void setSequence(const QList<int> &frameIndices, int fps = 12, bool loop = true);
    void clear();

    // Playback control
    void play();
    void pause();
    void stop();
    void togglePlayPause();

    // Stepping
    void stepForward();
    void stepBackward();
    void seek(int sequenceIndex);

    // Settings
    void setFps(int fps);
    int fps() const { return m_fps; }

    void setLoop(bool loop) { m_loop = loop; }
    bool isLooping() const { return m_loop; }

    bool isPlaying() const { return m_isPlaying; }
    int currentSequenceIndex() const { return m_currentIndex; }
    int currentGlobalFrameIndex() const;
    int frameCount() const { return m_frameIndices.size(); }

signals:
    void frameChanged(int sequenceIndex, int globalFrameIndex);
    void playbackStateChanged(bool isPlaying);

private slots:
    void onTick();

private:
    QTimer      m_timer;
    QList<int>  m_frameIndices;
    int         m_currentIndex = 0;
    int         m_fps = 12;
    bool        m_loop = true;
    bool        m_isPlaying = false;
};

#endif // ANIMATIONPLAYER_H
