#ifndef SPRITEDOCUMENT_H
#define SPRITEDOCUMENT_H

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QList>
#include <QMap>
#include <QRect>
#include <QString>
#include <QStringList>

/**
 * @brief Structure representing the bounding box of a sprite frame in the atlas.
 */
struct SpriteBox {
    QRect       rect;
    bool        selected = false;
    int         index = 0;
    int         groupId = 0;
    QList<int>  overlappingBoxes;

    bool operator==(const SpriteBox &other) const {
        return rect == other.rect && index == other.index && selected == other.selected;
    }
};

/**
 * @brief Structure representing an animation sequence.
 */
struct SpriteAnimation {
    QString     name;
    QList<int>  frameIndices;
    int         fps = 12;
    bool        loop = true;
};

/**
 * @brief SpriteDocument represents the central data model for a SpriteStudio project.
 *
 * It manages the raw atlas image, individual sliced frames, bounding boxes,
 * and named animations. It emits signals whenever the document content changes.
 */
class SpriteDocument : public QObject
{
    Q_OBJECT

public:
    explicit SpriteDocument(QObject *parent = nullptr);
    ~SpriteDocument() override = default;

    // Reset / Initialization
    void clear();
    bool isEmpty() const { return m_frames.isEmpty() && m_atlas.isNull(); }

    // File path & project info
    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path) { m_filePath = path; }
    QString projectName() const;

    // Atlas
    const QImage& atlas() const { return m_atlas; }
    void setAtlas(const QImage &image);

    // Frames
    int frameCount() const { return m_frames.size(); }
    const QList<QPixmap>& frames() const { return m_frames; }
    QPixmap frame(int index) const;
    void setFrames(const QList<QPixmap> &frames, const QList<SpriteBox> &boxes);
    void addFrame(const QPixmap &pixmap, const SpriteBox &box = SpriteBox());
    void insertFrame(int index, const QPixmap &pixmap, const SpriteBox &box);
    void replaceFrame(int index, const QPixmap &pixmap, const SpriteBox &box = SpriteBox());
    void removeFrame(int index);
    void removeFrames(const QList<int> &indices);
    void reorderFrames(const QList<int> &newOrder);
    void mergeFrames(int sourceIndex, int targetIndex);

    // Bounding Boxes
    const QList<SpriteBox>& boxes() const { return m_boxes; }
    SpriteBox box(int index) const;
    void setBox(int index, const SpriteBox &box);
    void updateBoxRect(int index, const QRect &newRect);
    int addSlice(const QRect &rect);
    QRect computeTrimmedRect(int index, int alphaThreshold = 1) const;
    void setBoxSelection(int index, bool selected);
    void setFrameSelected(int index, bool selected) { setBoxSelection(index, selected); }
    void clearBoxSelections();
    QList<int> selectedFrameIndices() const;

    // Dimensions
    int maxFrameWidth() const { return m_maxFrameWidth; }
    int maxFrameHeight() const { return m_maxFrameHeight; }

    // Animations
    QStringList animationNames() const { return m_animations.keys(); }
    bool hasAnimation(const QString &name) const { return m_animations.contains(name); }
    SpriteAnimation animation(const QString &name) const;
    const QMap<QString, SpriteAnimation>& animations() const { return m_animations; }
    void setAnimation(const QString &name, const QList<int> &frameIndices, int fps = 12, bool loop = true);
    void removeAnimation(const QString &name);
    void renameAnimation(const QString &oldName, const QString &newName);
    void reverseAnimationFrames(const QString &name);

    // Manipulation helper
    void clearAtlasAreas(const QList<int> &frameIndices);

signals:
    void atlasChanged();
    void framesChanged();
    void frameUpdated(int index);
    void animationsChanged();
    void documentReset();

private:
    void recalculateMaxFrameDimensions();

    QImage                          m_atlas;
    QList<QPixmap>                  m_frames;
    QList<SpriteBox>                m_boxes;
    QMap<QString, SpriteAnimation>  m_animations;
    QString                         m_filePath;
    int                             m_maxFrameWidth = 0;
    int                             m_maxFrameHeight = 0;
};

#endif // SPRITEDOCUMENT_H
