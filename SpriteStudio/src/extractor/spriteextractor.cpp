#include "extractor/spriteextractor.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <vector>

SpriteExtractor::SpriteExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool SpriteExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    return supportedExtensions().contains(fi.suffix().toLower());
}

bool SpriteExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    setStatusMessage(tr("Loading image %1...").arg(QFileInfo(filePath).fileName()));
    setProgress(5);

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("File not found: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QImage image(filePath);
    if (image.isNull()) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Failed to decode image from: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    bool ok = extractFromImage(image, outDoc, m_options);
    if (ok) {
        outDoc.setFilePath(filePath);
    }
    return ok;
}

bool SpriteExtractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(options);
    if (inDoc.atlas().isNull()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Cannot export: Document atlas image is null.");
            error->filePath = filePath;
        }
        return false;
    }

    QFileInfo fi(filePath);
    QString format = fi.suffix().toUpper();
    if (format.isEmpty()) format = QStringLiteral("PNG");

    if (!inDoc.atlas().save(filePath, format.toLatin1().constData())) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to save image to: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    setStatusMessage(tr("Exported atlas image to: %1").arg(filePath));
    return true;
}

#include "image/spritedetector.h"
#include "config/appconfig.h"

bool SpriteExtractor::extractFromImage(const QImage &image, SpriteDocument &outDoc, int alphaThreshold, int verticalTolerance)
{
    SpriteSheetOptions opts = m_options;
    opts.alphaThreshold = alphaThreshold;
    opts.verticalTolerance = verticalTolerance;
    return extractFromImage(image, outDoc, opts);
}

bool SpriteExtractor::extractToImages(const QImage &sourceImage,
                                     QList<QImage> &outFrames,
                                     QList<SpriteBox> &outBoxes,
                                     const SpriteSheetOptions &options)
{
    setStatusMessage(tr("Segmenting sprite frames..."));
    setProgress(15);

    SpriteDetectionOptions detOpts;
    detOpts.alphaThreshold = options.alphaThreshold;
    detOpts.verticalTolerance = options.verticalTolerance;
    detOpts.minSliceSize = AppConfig::instance().atlas().minSliceSize;
    detOpts.smartCrop = options.smartCrop;
    detOpts.overlapThreshold = options.overlapThreshold;

    bool ok = SpriteDetector::detectToImages(sourceImage, outFrames, outBoxes, detOpts, [this](int p) {
        setProgress(p);
    });

    if (ok) {
        setStatusMessage(tr("Extracted %1 frames").arg(outFrames.size()));
        emit extractionFinished(outFrames.size());
    }
    return ok;
}

bool SpriteExtractor::extractFromImage(const QImage &sourceImage, SpriteDocument &outDoc, const SpriteSheetOptions &options)
{
    QList<QImage> frameImages;
    QList<SpriteBox> boxes;

    if (!extractToImages(sourceImage, frameImages, boxes, options)) {
        return false;
    }

    QList<QPixmap> frames;
    frames.reserve(frameImages.size());
    for (const QImage &img : frameImages) {
        frames.append(QPixmap::fromImage(img));
    }

    QImage atlasImg = (sourceImage.format() == QImage::Format_ARGB32)
        ? sourceImage
        : sourceImage.convertToFormat(QImage::Format_ARGB32);

    outDoc.setAtlas(atlasImg);
    outDoc.setFrames(frames, boxes);
    return true;
}
