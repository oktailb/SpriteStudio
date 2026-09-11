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
#include <QMap>
#include <cmath>

ProjectController::ProjectController(SpriteDocument *document, QUndoStack *undoStack, QObject *parent)
    : QObject(parent)
    , m_document(document)
    , m_undoStack(undoStack)
{
    // Ensure extractor registry is initialized
    ExtractorRegistry::instance();
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
    int minAlpha = AppConfig::instance().project().backgroundMinAlpha;

    QImage image = srcImage.convertToFormat(QImage::Format_ARGB32);

    // Find the most frequent color by sampling
    QMap<QRgb, int> histogram;
    int maxCount = 0;
    QRgb backgroundColor = 0;

    for (int y = 0; y < image.height(); y += 2) {
        for (int x = 0; x < image.width(); x += 2) {
            QRgb pixel = image.pixel(x, y);
            if (qAlpha(pixel) < minAlpha) continue;

            histogram[pixel]++;
            if (histogram[pixel] > maxCount) {
                maxCount = histogram[pixel];
                backgroundColor = pixel;
            }
        }
    }

    if (maxCount == 0) return image;

    int bgR = qRed(backgroundColor);
    int bgG = qGreen(backgroundColor);
    int bgB = qBlue(backgroundColor);

    for (int y = 0; y < image.height(); ++y) {
        QRgb *scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
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

    m_document->setAtlas(cleanedImage);
    spriteExt.extractFromImage(cleanedImage, *m_document, alphaThreshold, verticalTolerance);

    emit backgroundRemoved();
    emit statusMessage(tr("Background removed."));
    return true;
}
