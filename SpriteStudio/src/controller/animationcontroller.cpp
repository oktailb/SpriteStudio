#include "include/controller/animationcontroller.h"
#include "include/animation/animationplayer.h"
#include "include/model/spritedocument.h"
#include "include/commands/commands.h"
#include "include/config/appconfig.h"
#include <QUndoStack>
#include <QInputDialog>
#include <QMessageBox>

AnimationController::AnimationController(SpriteDocument *document,
                                         QUndoStack *undoStack,
                                         AnimationPlayer *player,
                                         QTreeWidget *treeWidget,
                                         QGraphicsView *previewView,
                                         QObject *parent)
    : QObject(parent)
    , m_document(document)
    , m_undoStack(undoStack)
    , m_player(player)
    , m_ownsPlayer(false)
    , m_previewScene(new QGraphicsScene(this))
{
    if (!m_player) {
        m_player = new AnimationPlayer(this);
        m_ownsPlayer = true;
    }

    connect(m_player, &AnimationPlayer::frameChanged,
            this, &AnimationController::onPlayerFrameChanged);
    connect(m_player, &AnimationPlayer::playbackStateChanged,
            this, &AnimationController::onPlayerPlaybackStateChanged);

    if (m_document) {
        connect(m_document, &SpriteDocument::animationsChanged,
                this, &AnimationController::syncAnimationList);
        connect(m_document, &SpriteDocument::frameUpdated, this, [this](int globalIdx) {
            if (m_player && m_player->currentGlobalFrameIndex() == globalIdx) {
                renderCurrentFrame(globalIdx);
            }
        });
    }

    if (treeWidget) {
        attachTreeWidget(treeWidget);
    }

    if (previewView) {
        attachPreviewView(previewView);
    }
}

AnimationController::~AnimationController()
{
    if (m_ownsPlayer && m_player) {
        m_player->stop();
    }
}

void AnimationController::attachTreeWidget(QTreeWidget *treeWidget)
{
    m_treeWidget = treeWidget;
    if (!m_treeWidget) return;

    connect(m_treeWidget, &QTreeWidget::itemSelectionChanged,
            this, &AnimationController::onTreeItemSelectionChanged);
    connect(m_treeWidget, &QTreeWidget::itemClicked,
            this, &AnimationController::onTreeItemClicked);

    syncAnimationList();
}

void AnimationController::attachPreviewView(QGraphicsView *previewView)
{
    m_previewView = previewView;
    if (m_previewView) {
        m_previewView->setScene(m_previewScene);
    }
}

void AnimationController::play()
{
    if (m_player) {
        m_player->play();
    }
}

void AnimationController::pause()
{
    if (m_player) {
        m_player->pause();
    }
}

void AnimationController::stop()
{
    if (m_player) {
        m_player->stop();
        m_previewScene->clear();
        m_previewPixmapItem = nullptr;
    }
}

void AnimationController::togglePlayPause()
{
    if (m_player) {
        m_player->togglePlayPause();
    }
}

void AnimationController::stepForward()
{
    if (m_player) {
        m_player->stepForward();
    }
}

void AnimationController::stepBackward()
{
    if (m_player) {
        m_player->stepBackward();
    }
}

void AnimationController::seek(int sequenceIndex)
{
    if (m_player) {
        m_player->seek(sequenceIndex);
    }
}

bool AnimationController::isPlaying() const
{
    return m_player ? m_player->isPlaying() : false;
}

int AnimationController::fps() const
{
    return m_player ? m_player->fps() : AppConfig::instance().animation().defaultFps;
}

void AnimationController::setFps(int fps)
{
    const AnimationConfig &cfg = AppConfig::instance().animation();
    fps = std::clamp(fps, cfg.minFps, cfg.maxFps);
    if (m_player) {
        m_player->setFps(fps);
    }

    if (m_document && !m_currentAnimationName.isEmpty() && m_document->hasAnimation(m_currentAnimationName)) {
        SpriteAnimation anim = m_document->animation(m_currentAnimationName);
        if (anim.fps != fps) {
            m_document->setAnimation(m_currentAnimationName, anim.frameIndices, fps, anim.loop);
        }
    }

    emit fpsChanged(fps);
}

int AnimationController::currentSequenceIndex() const
{
    return m_player ? m_player->currentSequenceIndex() : 0;
}

int AnimationController::currentGlobalFrameIndex() const
{
    return m_player ? m_player->currentGlobalFrameIndex() : -1;
}

void AnimationController::selectAnimation(const QString &name)
{
    if (!m_document || !m_document->hasAnimation(name)) return;

    m_currentAnimationName = name;
    SpriteAnimation anim = m_document->animation(name);

    if (m_player) {
        m_player->setSequence(anim.frameIndices, anim.fps, anim.loop);
    }

    emit currentAnimationChanged(name);
    if (name != QLatin1String("current")) {
        emit framesSelectedInAnimation(anim.frameIndices);
    }

    // Synchronize tree widget selection if attached
    if (m_treeWidget) {
        m_treeWidget->blockSignals(true);
        QList<QTreeWidgetItem*> items = m_treeWidget->findItems(name, Qt::MatchExactly, 0);
        if (!items.isEmpty()) {
            m_treeWidget->setCurrentItem(items.first());
        }
        m_treeWidget->blockSignals(false);
    }

    updatePreview();
}

void AnimationController::createAnimation(const QString &name, const QList<int> &frameIndices, int fps)
{
    if (!m_document || name.isEmpty() || frameIndices.isEmpty()) return;
    if (fps <= 0) {
        fps = AppConfig::instance().animation().defaultFps;
    }

    if (name == QLatin1String("current")) {
        m_document->setAnimation(name, frameIndices, fps, true);
        selectAnimation(name);
        return;
    }

    if (m_undoStack) {
        m_undoStack->push(new CreateAnimationCommand(m_document, name, frameIndices, fps));
    } else {
        m_document->setAnimation(name, frameIndices, fps, true);
    }

    selectAnimation(name);
    play();
}

void AnimationController::createAnimationFromSelection(const QList<int> &selectedIndices)
{
    if (selectedIndices.isEmpty()) return;

    bool ok = false;
    QString name = QInputDialog::getText(nullptr, tr("New Animation"),
                                         tr("Animation name:"), QLineEdit::Normal,
                                         tr("anim_%1").arg(m_document ? m_document->animations().size() + 1 : 1),
                                         &ok);
    if (!ok || name.isEmpty()) return;

    createAnimation(name, selectedIndices, fps());
}

void AnimationController::removeAnimation(const QString &name)
{
    if (!m_document || !m_document->hasAnimation(name)) return;

    if (m_undoStack) {
        m_undoStack->push(new DeleteAnimationCommand(m_document, name));
    } else {
        m_document->removeAnimation(name);
    }

    if (m_currentAnimationName == name) {
        stop();
        m_currentAnimationName.clear();
    }
}

void AnimationController::removeAnimations(const QStringList &names)
{
    for (const QString &name : names) {
        removeAnimation(name);
    }
}

void AnimationController::removeSelectedAnimation()
{
    QStringList namesToDelete;
    if (m_treeWidget) {
        for (QTreeWidgetItem *item : m_treeWidget->selectedItems()) {
            namesToDelete.append(item->text(0));
        }
    } else if (!m_currentAnimationName.isEmpty()) {
        namesToDelete.append(m_currentAnimationName);
    }

    removeAnimations(namesToDelete);
}

void AnimationController::reverseAnimationOrder()
{
    QString targetName = m_currentAnimationName;
    if (m_treeWidget && !m_treeWidget->selectedItems().isEmpty()) {
        targetName = m_treeWidget->selectedItems().first()->text(0);
    }

    if (targetName.isEmpty() || !m_document || !m_document->hasAnimation(targetName)) {
        return;
    }

    if (m_undoStack) {
        m_undoStack->push(new ReverseAnimationCommand(m_document, targetName));
    } else {
        SpriteAnimation anim = m_document->animation(targetName);
        std::reverse(anim.frameIndices.begin(), anim.frameIndices.end());
        m_document->setAnimation(targetName, anim.frameIndices, anim.fps, anim.loop);
    }

    // Refresh sequence in player
    selectAnimation(targetName);
}

void AnimationController::updateCurrentAnimation(const QList<int> &selectedIndices)
{
    if (!m_document) return;

    if (selectedIndices.isEmpty()) {
        removeCurrentAnimation();
    } else {
        m_document->setAnimation(QStringLiteral("current"), selectedIndices, fps(), true);
        selectAnimation(QStringLiteral("current"));
    }
}

void AnimationController::removeCurrentAnimation()
{
    if (hasCurrentAnimation()) {
        m_document->removeAnimation(QStringLiteral("current"));
        if (m_currentAnimationName == QLatin1String("current")) {
            stop();
            m_currentAnimationName.clear();
        }
    }
}

bool AnimationController::hasCurrentAnimation() const
{
    return m_document && m_document->hasAnimation(QStringLiteral("current"));
}

void AnimationController::syncAnimationList()
{
    if (!m_treeWidget || !m_document) return;

    m_treeWidget->blockSignals(true);

    QString currentSelected = m_currentAnimationName;
    if (currentSelected.isEmpty() && !m_treeWidget->selectedItems().isEmpty()) {
        currentSelected = m_treeWidget->selectedItems().first()->text(0);
    }

    const auto &animMap = m_document->animations();

    // Check if we can do an in-place update to avoid clear() allocations
    bool canUpdateInPlace = (m_treeWidget->topLevelItemCount() == animMap.size());
    if (canUpdateInPlace) {
        for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
            if (!animMap.contains(m_treeWidget->topLevelItem(i)->text(0))) {
                canUpdateInPlace = false;
                break;
            }
        }
    }

    if (canUpdateInPlace) {
        for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = m_treeWidget->topLevelItem(i);
            QString name = item->text(0);
            const SpriteAnimation &anim = animMap.value(name);
            item->setText(1, QString::number(anim.fps));
            QStringList framesStrList;
            for (int frameIndex : anim.frameIndices) {
                framesStrList << QString::number(frameIndex + 1);
            }
            item->setText(2, framesStrList.join(QStringLiteral(", ")));
            item->setSelected(name == currentSelected);
        }
    } else {
        m_treeWidget->clear();
        for (auto it = animMap.begin(); it != animMap.end(); ++it) {
            const QString &name = it.key();
            const SpriteAnimation &anim = it.value();

            QTreeWidgetItem *item = new QTreeWidgetItem(m_treeWidget);
            item->setText(0, name);
            item->setText(1, QString::number(anim.fps));

            QStringList framesStrList;
            for (int frameIndex : anim.frameIndices) {
                framesStrList << QString::number(frameIndex + 1);
            }
            item->setText(2, framesStrList.join(QStringLiteral(", ")));

            if (name == currentSelected) {
                item->setSelected(true);
            }
        }
    }

    m_treeWidget->blockSignals(false);
    emit animationListChanged();
}

void AnimationController::updatePreview()
{
    if (m_player) {
        int globalIdx = m_player->currentGlobalFrameIndex();
        if (globalIdx >= 0) {
            renderCurrentFrame(globalIdx);
        }
    }
}

void AnimationController::renderCurrentFrame(int globalFrameIndex)
{
    if (!m_document || globalFrameIndex < 0 || globalFrameIndex >= m_document->frameCount()) {
        return;
    }

    const QPixmap &currentFrame = m_document->frame(globalFrameIndex);
    if (currentFrame.isNull()) return;

    int maxWidth = qMax(1, m_document->maxFrameWidth());
    int maxHeight = qMax(1, m_document->maxFrameHeight());
    m_previewScene->setSceneRect(0, 0, maxWidth, maxHeight);

    if (!m_previewPixmapItem || m_previewPixmapItem->scene() != m_previewScene) {
        m_previewScene->clear();
        m_previewPixmapItem = m_previewScene->addPixmap(currentFrame);
    } else {
        m_previewPixmapItem->setPixmap(currentFrame);
    }

    qreal xOffset = (maxWidth - currentFrame.width()) / 2.0;
    qreal yOffset = (maxHeight - currentFrame.height()) / 2.0;
    m_previewPixmapItem->setPos(xOffset, yOffset);

    if (m_previewView) {
        m_previewView->viewport()->update();
    }
}

void AnimationController::onPlayerFrameChanged(int seqIndex, int globalIndex)
{
    renderCurrentFrame(globalIndex);
    emit frameChanged(seqIndex, globalIndex);
}

void AnimationController::onPlayerPlaybackStateChanged(bool playing)
{
    emit playbackStateChanged(playing);
}

void AnimationController::onTreeItemSelectionChanged()
{
    if (!m_treeWidget || m_treeWidget->selectedItems().isEmpty()) return;
    QString animName = m_treeWidget->selectedItems().first()->text(0);
    selectAnimation(animName);
}

void AnimationController::onTreeItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    selectAnimation(item->text(0));
}
