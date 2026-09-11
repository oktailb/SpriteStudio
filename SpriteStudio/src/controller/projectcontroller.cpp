#include "include/controller/projectcontroller.h"
#include "include/model/spritedocument.h"
#include "include/extractor/extractorregistry.h"
#include "include/extractor/spriteextractor.h"
#include "include/config/appconfig.h"
#include <QUndoStack>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QColor>
#include <QHash>
#include <QtConcurrent>
#include <cmath>
#include <vector>

ProjectController::ProjectController(SpriteDocument *document, QUndoStack *undoStack, QObject *parent)
    : QObject(parent)
    , m_document(document)
    , m_undoStack(undoStack)
{
    // Ensure extractor registry is initialized
    ExtractorRegistry::instance();

    connect(&m_watcher, &QFutureWatcher<AsyncExtractionResult>::finished,
            this, &ProjectController::onAsyncJobFinished);
}

QString ProjectController::currentFilePath() const
{
    return m_currentFilePath;
}

void ProjectController::setCurrentFilePath(const QString &filePath)
{
    m_currentFilePath = filePath;
}

bool ProjectController::openFile(const QString &filePath, QString *errorMsg)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        QString err = tr("File does not exist: %1").arg(filePath);
        if (errorMsg) *errorMsg = err;
        emit fileLoadError(filePath, err);
        return false;
    }

    Extractor *extractor = ExtractorRegistry::instance().findDecoder(filePath);
    if (!extractor) {
        QString err = tr("No suitable codec found for file: %1").arg(filePath);
        if (errorMsg) *errorMsg = err;
        emit fileLoadError(filePath, err);
        return false;
    }

    if (!m_document) {
        QString err = tr("No active SpriteDocument.");
        if (errorMsg) *errorMsg = err;
        emit fileLoadError(filePath, err);
        return false;
    }

    ExtractorError err;
    emit statusMessage(tr("Loading %1...").arg(QFileInfo(filePath).fileName()));

    if (!extractor->read(filePath, *m_document, &err)) {
        QString fullError = err.toString();
        if (errorMsg) *errorMsg = fullError;
        emit fileLoadError(filePath, fullError);
        return false;
    }

    m_currentFilePath = filePath;
    addRecentFile(filePath);

    if (m_undoStack) {
        m_undoStack->clear();
    }

    emit statusMessage(tr("Loaded %1 successfully.").arg(QFileInfo(filePath).fileName()));
    emit fileLoaded(filePath);
    return true;
}

void ProjectController::openFileAsync(const QString &filePath)
{
    if (m_isProcessing) return;

    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        QString err = tr("File does not exist: %1").arg(filePath);
        emit fileLoadError(filePath, err);
        return;
    }

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    // If it's a non-image file (e.g. JSON or GIF), fallback to synchronous read
    if (ext == QStringLiteral("json") || ext == QStringLiteral("tres") || ext == QStringLiteral("gif")) {
        emit processingStarted();
        openFile(filePath);
        emit processingFinished();
        return;
    }

    Extractor *extractor = ExtractorRegistry::instance().findDecoder(filePath);
    if (!extractor) {
        QString err = tr("No suitable codec found for file: %1").arg(filePath);
        emit fileLoadError(filePath, err);
        return;
    }

    m_isProcessing = true;
    emit processingStarted();
    emit statusMessage(tr("Loading %1 in background...").arg(fi.fileName()));
    emit progressChanged(20);

    QFuture<AsyncExtractionResult> future = QtConcurrent::run([filePath]() -> AsyncExtractionResult {
        AsyncExtractionResult result;
        result.type = AsyncExtractionResult::JobOpen;
        result.filePath = filePath;

        QImage image(filePath);
        if (image.isNull()) {
            result.success = false;
            result.errorMessage = QObject::tr("Failed to decode image from: %1").arg(filePath);
            return result;
        }

        SpriteExtractor spriteExt;
        if (!spriteExt.extractToImages(image, result.frameImages, result.boxes)) {
            result.success = false;
            result.errorMessage = QObject::tr("Failed to segment sprite frames.");
            return result;
        }

        result.atlas = (image.format() == QImage::Format_ARGB32) ? image : image.convertToFormat(QImage::Format_ARGB32);
        result.success = true;
        return result;
    });

    m_watcher.setFuture(future);
}

bool ProjectController::save(const QString &filePath, QString *errorMsg)
{
    return exportData(filePath, ExportOptions{}, errorMsg);
}

bool ProjectController::exportData(const QString &filePath, const ExportOptions &options, QString *errorMsg)
{
    if (filePath.isEmpty()) {
        QString err = tr("File path is empty.");
        if (errorMsg) *errorMsg = err;
        return false;
    }

    if (!m_document || m_document->isEmpty()) {
        QString err = tr("Document is empty.");
        if (errorMsg) *errorMsg = err;
        return false;
    }

    Extractor *extractor = ExtractorRegistry::instance().findEncoder(filePath);
    if (!extractor) {
        QString err = tr("No suitable exporter found for format: %1").arg(filePath);
        if (errorMsg) *errorMsg = err;
        return false;
    }

    ExtractorError err;
    emit statusMessage(tr("Saving %1...").arg(QFileInfo(filePath).fileName()));

    if (!extractor->write(filePath, *m_document, options, &err)) {
        QString fullError = err.toString();
        if (errorMsg) *errorMsg = fullError;
        return false;
    }

    m_currentFilePath = filePath;
    addRecentFile(filePath);
    emit statusMessage(tr("Saved %1 successfully.").arg(QFileInfo(filePath).fileName()));
    emit fileSaved(filePath);
    return true;
}

QStringList ProjectController::recentFiles() const
{
    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    QStringList files = settings.value(QStringLiteral("recentFiles")).toStringList();

    QStringList existingFiles;
    for (const QString &f : files) {
        if (QFile::exists(f)) {
            existingFiles.append(f);
        }
    }
    return existingFiles;
}

void ProjectController::addRecentFile(const QString &filePath)
{
    if (filePath.isEmpty()) return;

    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    QStringList files = settings.value(QStringLiteral("recentFiles")).toStringList();
    files.removeAll(filePath);
    files.prepend(filePath);
    int maxFiles = AppConfig::instance().project().maxRecentFiles;
    while (files.size() > maxFiles) {
        files.removeLast();
    }
    settings.setValue(QStringLiteral("recentFiles"), files);
    emit recentFilesChanged(recentFiles());
}

void ProjectController::clearRecentFiles()
{
    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    settings.remove(QStringLiteral("recentFiles"));
    emit recentFilesChanged(QStringList());
}

QImage ProjectController::removeBackgroundFromImage(const QImage &srcImage, int tolerance)
{
    if (srcImage.isNull()) return QImage();

    if (tolerance < 0) {
        tolerance = AppConfig::instance().project().backgroundRemovalTolerance;
    }
    const int minAlpha = AppConfig::instance().project().backgroundMinAlpha;

    QImage image = (srcImage.format() == QImage::Format_ARGB32)
        ? srcImage.copy()
        : srcImage.convertToFormat(QImage::Format_ARGB32);

    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0) return image;

    std::vector<const QRgb*> constScanLines(h);
    for (int y = 0; y < h; ++y) {
        constScanLines[y] = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    }

    // Find the most frequent color by sampling with O(1) hash map
    QHash<QRgb, int> histogram;
    histogram.reserve(4096);
    int maxCount = 0;
    QRgb backgroundColor = 0;

    for (int y = 0; y < h; y += 2) {
        const QRgb *line = constScanLines[y];
        for (int x = 0; x < w; x += 2) {
            QRgb pixel = line[x];
            if (qAlpha(pixel) < minAlpha) continue;

            int &count = histogram[pixel];
            count++;
            if (count > maxCount) {
                maxCount = count;
                backgroundColor = pixel;
            }
        }
    }

    if (maxCount == 0) return image;

    const int bgR = qRed(backgroundColor);
    const int bgG = qGreen(backgroundColor);
    const int bgB = qBlue(backgroundColor);

    for (int y = 0; y < h; ++y) {
        QRgb *scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb pixel = scanLine[x];
            if (qAlpha(pixel) < minAlpha) continue;

            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            if (std::abs(r - bgR) <= tolerance &&
                std::abs(g - bgG) <= tolerance &&
                std::abs(b - bgB) <= tolerance) {
                scanLine[x] = qRgba(0, 0, 0, 0);
            }
        }
    }

    return image;
}

bool ProjectController::removeAtlasBackgroundAndRefresh(int alphaThreshold,
                                                        int verticalTolerance,
                                                        bool smartCrop,
                                                        double overlapThreshold)
{
    if (!m_document || m_document->atlas().isNull()) return false;

    emit statusMessage(tr("Removing background..."));
    int defaultTol = AppConfig::instance().project().backgroundRemovalTolerance;
    QImage cleanedImage = removeBackgroundFromImage(m_document->atlas(), defaultTol);

    SpriteExtractor spriteExt;
    spriteExt.setSmartCropEnabled(smartCrop);
    spriteExt.setOverlapThreshold(overlapThreshold);

    if (alphaThreshold < 0) {
        alphaThreshold = AppConfig::instance().atlas().defaultAlphaThreshold;
    }

    QList<QImage> frameImages;
    QList<SpriteBox> boxes;
    SpriteSheetOptions opts;
    opts.alphaThreshold = alphaThreshold;
    opts.verticalTolerance = verticalTolerance;
    opts.smartCrop = smartCrop;
    opts.overlapThreshold = overlapThreshold;

    if (!spriteExt.extractToImages(cleanedImage, frameImages, boxes, opts)) {
        return false;
    }

    QList<QPixmap> frames;
    frames.reserve(frameImages.size());
    for (const QImage &img : frameImages) {
        frames.append(QPixmap::fromImage(img));
    }

    m_document->setAtlas(cleanedImage);
    m_document->setFrames(frames, boxes);

    emit backgroundRemoved();
    emit statusMessage(tr("Background removed."));
    return true;
}

void ProjectController::removeAtlasBackgroundAndRefreshAsync(int alphaThreshold,
                                                            int verticalTolerance,
                                                            bool smartCrop,
                                                            double overlapThreshold)
{
    if (m_isProcessing || !m_document || m_document->atlas().isNull()) return;

    m_isProcessing = true;
    emit processingStarted();
    emit statusMessage(tr("Removing background in background..."));
    emit progressChanged(20);

    QImage atlasCopy = m_document->atlas();
    const int defaultTol = AppConfig::instance().project().backgroundRemovalTolerance;
    const int actualAlpha = (alphaThreshold < 0) ? AppConfig::instance().atlas().defaultAlphaThreshold : alphaThreshold;

    QFuture<AsyncExtractionResult> future = QtConcurrent::run([atlasCopy, defaultTol, actualAlpha, verticalTolerance, smartCrop, overlapThreshold]() -> AsyncExtractionResult {
        AsyncExtractionResult result;
        result.type = AsyncExtractionResult::JobRemoveBackground;

        QImage cleaned = ProjectController::removeBackgroundFromImage(atlasCopy, defaultTol);
        if (cleaned.isNull()) {
            result.success = false;
            result.errorMessage = QObject::tr("Failed to remove background from atlas.");
            return result;
        }

        SpriteExtractor spriteExt;
        spriteExt.setSmartCropEnabled(smartCrop);
        spriteExt.setOverlapThreshold(overlapThreshold);

        SpriteSheetOptions opts;
        opts.alphaThreshold = actualAlpha;
        opts.verticalTolerance = verticalTolerance;
        opts.smartCrop = smartCrop;
        opts.overlapThreshold = overlapThreshold;

        if (!spriteExt.extractToImages(cleaned, result.frameImages, result.boxes, opts)) {
            result.success = false;
            result.errorMessage = QObject::tr("Failed to segment frames after background removal.");
            return result;
        }

        result.atlas = cleaned;
        result.success = true;
        return result;
    });

    m_watcher.setFuture(future);
}

void ProjectController::onAsyncJobFinished()
{
    AsyncExtractionResult res = m_watcher.result();
    m_isProcessing = false;
    emit processingFinished();

    if (!res.success) {
        emit progressChanged(0);
        if (res.type == AsyncExtractionResult::JobOpen) {
            emit fileLoadError(res.filePath, res.errorMessage);
        } else {
            emit statusMessage(res.errorMessage);
        }
        return;
    }

    // Convert QImage frames to QPixmap on GUI thread
    QList<QPixmap> frames;
    frames.reserve(res.frameImages.size());
    for (const QImage &img : res.frameImages) {
        frames.append(QPixmap::fromImage(img));
    }

    if (m_document) {
        m_document->setAtlas(res.atlas);
        m_document->setFrames(frames, res.boxes);
    }

    if (res.type == AsyncExtractionResult::JobOpen) {
        m_currentFilePath = res.filePath;
        addRecentFile(res.filePath);
        if (m_undoStack) {
            m_undoStack->clear();
        }
        emit statusMessage(tr("Loaded %1 successfully.").arg(QFileInfo(res.filePath).fileName()));
        emit progressChanged(100);
        emit fileLoaded(res.filePath);
    } else if (res.type == AsyncExtractionResult::JobRemoveBackground) {
        emit statusMessage(tr("Background removed successfully."));
        emit progressChanged(100);
        emit backgroundRemoved();
    }
}
