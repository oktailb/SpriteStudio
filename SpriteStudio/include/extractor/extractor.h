/**
 * @file extractor.h
 * @brief Defines the abstract base class Extractor, the interface for all sprite extraction types.
 */

#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <QObject>
#include <QPixmap>
#include <QList>
#include <QMap>
#include <QString>
#include <QPainter>
#include "export.h"

/**
 * @brief Abstract base class (interface) for all sprite extractors.
 *
 * This class defines the common interface that all sprite import mechanisms
 * must implement (e.g., SpriteExtractor for static sheets, GifExtractor for animated GIFs, etc.).
 * It manages the central state of the extracted data: individual frames, the composite atlas,
 * and maximum frame size metadata.
 *
 * It inherits from QObject to enable signal/slot mechanisms (essential for asynchronous operations like GIFs).
 */
class Extractor : public QObject, ExportManager
{
    Q_OBJECT
public:
    /**
     * @brief Constructor for the Extractor class.
     * @param parent The parent QObject.
     */
    explicit Extractor(QObject *parent = nullptr) : QObject(parent)
    {
      m_maxFrameWidth = 0;
      m_maxFrameHeight = 0;
    }

    /**
     * @brief Ajoute ou met à jour une animation
     * @param name Nom de l'animation
     * @param frameIndices Liste des indices de frames
     * @param fps Images par seconde
     */
    void setAnimation(const QString &name, const QList<int> &frameIndices, int fps = 60);

    /**
     * @brief Supprime une animation
     * @param name Nom de l'animation à supprimer
     */
    void removeAnimation(const QString &name);

    /**
     * @brief Récupère les indices de frames d'une animation
     * @param name Nom de l'animation
     * @return Liste des indices de frames
     */
    QList<int> getAnimationFrames(const QString &name) const;

    /**
     * @brief Récupère le FPS d'une animation
     * @param name Nom de l'animation
     * @return FPS de l'animation
     */
    int getAnimationFps(const QString &name) const;

    /**
     * @brief Récupère tous les noms d'animations
     * @return Liste des noms d'animations
     */
    QStringList getAnimationNames() const;

    /**
     * @brief Met à jour l'ordre des frames
     * @param newOrder Nouvel ordre des indices de frames
     */
    void reorderFrames(const QList<int> &newOrder);

    /**
     * @brief Inverse l'ordre des frames d'une animation
     * @param animationName Nom de l'animation à inverser
     */
    void reverseAnimationFrames(const QString &animationName);

    /**
     * @brief Supprime une frame par son index
     * @param index Index de la frame à supprimer
     */
    void removeFrame(int index);

    /**
     * @brief removeFrames
     * @param indices
     */
    void removeFrames(const QList<int> &indices);

    /**
     * @brief clearAtlasAreas
     * @param indices
     */
    void clearAtlasAreas(const QList<int> &indices);

    /**
     * @brief Extracts frames from the source file.
     *
     * This is the primary function for loading and slicing. It is implemented
     * differently by each subclass (e.g., alpha detection for SpriteExtractor,
     * sequential reading for GifExtractor).
     *
     * @param filePath Path to the source file (PNG, GIF, etc.).
     * @param alphaThreshold Alpha threshold used for edge detection (used by SpriteExtractor).
     * @param verticalTolerance Vertical tolerance between sprites (used by SpriteExtractor).
     * @return QList of QPixmap containing all extracted frames.
     */
    virtual QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance) = 0;

    /**
     * @brief Re-runs the extraction algorithm on the atlas currently held in memory (m_atlas).
     *
     * Used to update the frames after an in-memory modification of the atlas (e.g., background removal)
     * or a change in detection parameters (e.g., verticalTolerance).
     *
     * @note This is a pure virtual function. For non-detection based extractors (GifExtractor, JsonExtractor),
     * it should be implemented as a no-operation (no-op).
     *
     * @param alphaThreshold Alpha threshold.
     * @param verticalTolerance Vertical tolerance.
     * @return QList of QPixmap containing the re-extracted frames.
     */
    virtual QList<QPixmap> extractFromPixmap(int alphaThreshold, int verticalTolerance) = 0;

    /**
     * @brief Exports the extracted frames and metadata to a specified format.
     *
     * The export logic is often delegated to a specific exporter implementation (e.g., JsonExtractor for JSON format).
     *
     * @param basePath Base path for saving the output files.
     * @param projectName Base name for the files (without extension).
     * @param in Pointer to the source extractor containing the data to be exported.
     * @return true if the export was successful, false otherwise.
     */
    virtual bool           exportFrames(const QString &basePath, const QString &projectName, Extractor* in) = 0;

    /**
     * @brief Adds a frame to the internal m_frames list.
     * @param pixmap The QPixmap frame to add.
     */
    void addFrame(const QPixmap &pixmap)
    {
        if (!pixmap.isNull()) {
            m_frames.append(pixmap);
        }
    }

    /**
     * @brief Structure representing the Bounding Box of a sprite within the atlas.
     *
     * These coordinates (x, y, w, h) are used to generate the metadata file (e.g., JSON)
     * during the export process.
     */
    struct Box {
        QRect rect;
//        QPolygon polygon;
        bool selected;
        int index;
        int groupId;
        QList<int> overlappingBoxes;
    };

    enum CropStrategy {
        MergeStrategy,      // Fusionne les sprites qui se chevauchent
        SeparateStrategy,   // Sépare en utilisant la connectivité
        BoundaryStrategy,   // Utilise les limites naturelles
        AlphaChannelStrategy // Sépare basé sur le canal alpha
    };

    struct AnimationData {
        QList<int> frameIndices;
        int fps;
    };

    QList<QPixmap>              m_frames;         /**< List of individual frames (used by the Frame List and Animation). */
    QImage                      m_atlas;          /**< The complete source image (possibly composite, used by the 'Layers' view). */
    QList<Box>                  m_atlas_index;    /**< List of Box coordinates for each frame within m_atlas. */
    QMap<QString, AnimationData> m_animationsData; /**< List of individual animations */
    QString                     m_filePath;       /**< The original data file from a supported format. */
    int                         m_maxFrameWidth;  /**< Maximum width among all extracted frames (used for animation bounding box). */
    int                         m_maxFrameHeight; /**< Maximum height among all extracted frames (used for animation bounding box). */
    ExportOptions               m_opts;

    bool m_smartCropEnabled = true;
    double m_overlapThreshold = 0.1; // 10% de chevauchement minimum
    CropStrategy m_cropStrategy = SeparateStrategy;

    void setSmartCropEnabled(bool newSmartCropEnabled);

    bool smartCropEnabled() const;

    double overlapThreshold() const;
    void setOverlapThreshold(double newOverlapThreshold);

    ExportOptions opts() const;

signals:
    /**
     * @brief Signal emitted when the frame extraction process is complete.
     * @param frameCount The total number of frames successfully extracted.
     */
    void            extractionFinished(int frameCount);

};

#endif // EXTRACTOR_H
