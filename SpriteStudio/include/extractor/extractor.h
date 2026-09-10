/**
 * @file extractor.h
 * @brief Defines the abstract base class Extractor, the interface for all sprite extraction types and plugins.
 */

#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <QObject>
#include <QPixmap>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QPainter>
#include <QLabel>
#include <QProgressBar>
#include "export.h"

class SpriteDocument;

/**
 * @brief Abstract base class and unified plugin interface for sprite extractors.
 *
 * This class defines the common interface that all sprite import/export mechanisms
 * must implement (e.g., SpriteExtractor for static sheets, GifExtractor for animated GIFs,
 * JsonExtractor for JSON atlas databases, GodotExtractor for Godot SpriteFrames).
 *
 * It manages the central state of extracted data: individual frames, the composite atlas,
 * bounding boxes, and animations. It also bridges seamlessly to SpriteDocument.
 */
class Extractor : public QObject, public ExportManager
{
    Q_OBJECT

public:
    /**
     * @brief Structure representing the Bounding Box of a sprite within the atlas.
     */
    struct Box {
        QRect       rect;
        bool        selected = false;
        int         index = 0;
        int         groupId = 0;
        QList<int>  overlappingBoxes;
    };

    enum CropStrategy {
        MergeStrategy,
        SeparateStrategy,
        BoundaryStrategy,
        AlphaChannelStrategy
    };

    struct AnimationData {
        QList<int>  frameIndices;
        int         fps = 60;
    };

    enum Capability {
        CanImport             = 0x01,
        CanExport             = 0x02,
        SupportsAnimations    = 0x04,
        SupportsAtlasMetadata = 0x08,
        SupportsMultiAtlas    = 0x10
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    explicit Extractor(QObject *parent = nullptr);
    explicit Extractor(QLabel *statusBar, QProgressBar *progressBar, QObject *parent = nullptr);
    ~Extractor() override = default;

    // Plugin metadata & capabilities
    virtual QString id() const { return QString(); }
    virtual QString displayName() const { return QString(); }
    virtual QString description() const { return QString(); }
    virtual QStringList supportedExtensions() const { return QStringList(); }
    virtual Capabilities capabilities() const { return CanImport; }
    virtual bool canDecode(const QString &filePath) const {
        Q_UNUSED(filePath);
        return false;
    }

    // Unified Document extraction and export operations
    virtual bool extract(const QString &filePath, SpriteDocument &doc, QString *errorMsg = nullptr);
    virtual bool exportDocument(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, QString *errorMsg = nullptr);

    // Bridge helpers with SpriteDocument
    void syncToDocument(SpriteDocument &doc) const;
    void syncFromDocument(const SpriteDocument &doc);

    // Legacy / direct extractor virtual methods
    virtual QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance) = 0;
    virtual QList<QPixmap> extractFromPixmap(int alphaThreshold, int verticalTolerance) = 0;
    virtual bool exportFrames(const QString &basePath, const QString &projectName, Extractor* in) = 0;

    // Animation management
    void setAnimation(const QString &name, const QList<int> &frameIndices, int fps = 60);
    void removeAnimation(const QString &name);
    QList<int> getAnimationFrames(const QString &name) const;
    int getAnimationFps(const QString &name) const;
    QStringList getAnimationNames() const;

    // Frame manipulation
    void reorderFrames(const QList<int> &newOrder);
    void reverseAnimationFrames(const QString &animationName);
    void removeFrame(int index);
    void removeFrames(const QList<int> &indices);
    void clearAtlasAreas(const QList<int> &indices);

    void addFrame(const QPixmap &pixmap) {
        if (!pixmap.isNull()) {
            m_frames.append(pixmap);
        }
    }

    void setSmartCropEnabled(bool newSmartCropEnabled);
    bool smartCropEnabled() const;
    double overlapThreshold() const;
    void setOverlapThreshold(double newOverlapThreshold);
    ExportOptions opts() const;

    // Member variables
    QList<QPixmap>                m_frames;
    QImage                        m_atlas;
    QList<Box>                    m_atlas_index;
    QMap<QString, AnimationData>  m_animationsData;
    QString                       m_filePath;
    int                           m_maxFrameWidth = 0;
    int                           m_maxFrameHeight = 0;
    ExportOptions                 m_opts;
    QLabel *                      m_statusBar = nullptr;
    QProgressBar *                m_progressBar = nullptr;
    bool                          m_smartCropEnabled = true;
    double                        m_overlapThreshold = 0.1;
    CropStrategy                  m_cropStrategy = SeparateStrategy;

signals:
    void progress(int percentage);
    void statusMessage(const QString &message);
    void extractionFinished(int frameCount);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Extractor::Capabilities)

#define Extractor_iid "com.spritestudio.Extractor/1.0"
Q_DECLARE_INTERFACE(Extractor, Extractor_iid)

#endif // EXTRACTOR_H
