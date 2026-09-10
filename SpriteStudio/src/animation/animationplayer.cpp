#include "animation/animationplayer.h"

AnimationPlayer::AnimationPlayer(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &AnimationPlayer::onTick);
}

void AnimationPlayer::setSequence(const QList<int> &frameIndices, int fps, bool loop)
{
    m_frameIndices = frameIndices;
    m_fps = (fps > 0) ? fps : 12;
    m_loop = loop;
    m_currentIndex = 0;

    if (m_timer.isActive()) {
        m_timer.setInterval(1000 / m_fps);
    }

    emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
}

void AnimationPlayer::clear()
{
    stop();
    m_frameIndices.clear();
    m_currentIndex = 0;
    emit frameChanged(0, -1);
}

void AnimationPlayer::play()
{
    if (m_frameIndices.isEmpty()) return;

    m_isPlaying = true;
    m_timer.start(1000 / m_fps);
    emit playbackStateChanged(true);
}

void AnimationPlayer::pause()
{
    if (m_isPlaying) {
        m_isPlaying = false;
        m_timer.stop();
        emit playbackStateChanged(false);
    }
}

void AnimationPlayer::stop()
{
    m_isPlaying = false;
    m_timer.stop();
    m_currentIndex = 0;
    emit playbackStateChanged(false);
    emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
}

void AnimationPlayer::togglePlayPause()
{
    if (m_isPlaying) {
        pause();
    } else {
        play();
    }
}

void AnimationPlayer::stepForward()
{
    if (m_frameIndices.isEmpty()) return;
    pause();
    m_currentIndex = (m_currentIndex + 1) % m_frameIndices.size();
    emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
}

void AnimationPlayer::stepBackward()
{
    if (m_frameIndices.isEmpty()) return;
    pause();
    m_currentIndex = (m_currentIndex - 1 + m_frameIndices.size()) % m_frameIndices.size();
    emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
}

void AnimationPlayer::seek(int sequenceIndex)
{
    if (sequenceIndex >= 0 && sequenceIndex < m_frameIndices.size()) {
        m_currentIndex = sequenceIndex;
        emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
    }
}

void AnimationPlayer::setFps(int fps)
{
    if (fps <= 0) return;
    m_fps = fps;
    if (m_timer.isActive()) {
        m_timer.setInterval(1000 / m_fps);
    }
}

int AnimationPlayer::currentGlobalFrameIndex() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_frameIndices.size()) {
        return m_frameIndices.at(m_currentIndex);
    }
    return -1;
}

void AnimationPlayer::onTick()
{
    if (m_frameIndices.isEmpty()) {
        stop();
        return;
    }

    m_currentIndex++;
    if (m_currentIndex >= m_frameIndices.size()) {
        if (m_loop) {
            m_currentIndex = 0;
        } else {
            stop();
            return;
        }
    }

    emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
}
