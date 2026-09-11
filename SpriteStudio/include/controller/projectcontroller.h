#ifndef PROJECTCONTROLLER_H
#define PROJECTCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QImage>
#include "extractor/export.h"

class SpriteDocument;
class QUndoStack;

/**
 * @brief Controller managing project lifecycle, I/O operations, recent files, and image processing.
 */
class ProjectController : public QObject
{
    Q_OBJECT

public:
    explicit ProjectController(SpriteDocument *document, QUndoStack *undoStack = nullptr, QObject *parent = nullptr);
    ~ProjectController() override = default;

    QString currentFilePath() const;
    void setCurrentFilePath(const QString &filePath);

    bool openFile(const QString &filePath, QString *errorMsg = nullptr);
    bool save(const QString &filePath, QString *errorMsg = nullptr);
    bool exportData(const QString &filePath, const ExportOptions &options = ExportOptions{}, QString *errorMsg = nullptr);

    QStringList recentFiles() const;
    void addRecentFile(const QString &filePath);
    void clearRecentFiles();

    /**
     * @brief Detects the dominant background color and turns matching pixels transparent.
     * @param image Input image.
     * @param tolerance Color difference tolerance (0-255).
     * @return Processed image with transparent background.
     */
    static QImage removeBackgroundFromImage(const QImage &image, int tolerance = -1);

    /**
     * @brief Removes the background from the document's atlas and re-extracts sprites.
     */
    bool removeAtlasBackgroundAndRefresh(int alphaThreshold = 10,
                                         int verticalTolerance = 5,
                                         bool smartCrop = false,
                                         double overlapThreshold = 0.5);

signals:
    void fileLoaded(const QString &filePath);
    void fileLoadError(const QString &filePath, const QString &errorMessage);
    void fileSaved(const QString &filePath);
    void recentFilesChanged(const QStringList &files);
    void statusMessage(const QString &message);
    void progressChanged(int percent);
    void backgroundRemoved();

private:
    SpriteDocument *m_document;
    QUndoStack *m_undoStack;
    QString m_currentFilePath;
};

#endif // PROJECTCONTROLLER_H
