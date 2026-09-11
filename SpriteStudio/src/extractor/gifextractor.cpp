#include "extractor/gifextractor.h"
#include "packer/atlaspacker.h"
#include <QImageReader>
#include <QFileInfo>
#include <QDebug>
#include <cmath>

GifExtractor::GifExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool GifExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    return fi.suffix().toLower() == QStringLiteral("gif");
}

bool GifExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    setStatusMessage(tr("Reading GIF frames from %1...").arg(QFileInfo(filePath).fileName()));
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

    QImageReader reader(filePath);
    if (!reader.canRead()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("Unable to read GIF format: %1").arg(reader.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    int expectedCount = reader.imageCount();
    QList<QPixmap> framePixmaps;
    int totalDelayMs = 0;
    int frameIndex = 0;

    while (reader.canRead()) {
        int delay = reader.nextImageDelay();
        if (delay <= 0) delay = 100; // default 10 fps
        totalDelayMs += delay;

        QImage frameImg = reader.read();
        if (frameImg.isNull()) break;

        framePixmaps.append(QPixmap::fromImage(frameImg));
        frameIndex++;

        if (expectedCount > 0) {
            setProgress(qMin(70, 5 + (65 * frameIndex / expectedCount)));
        }
    }

    if (framePixmaps.isEmpty()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("No valid frames could be decoded from GIF: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    setStatusMessage(tr("Assembling GIF atlas..."));
    setProgress(75);

    // Pack frames into an atlas
    AtlasPackResult packResult = AtlasPacker::pack(framePixmaps, 2);
    if (!packResult.success) {
        if (error) {
            error->code = ExtractorError::PackingFailed;
            error->message = tr("Failed to pack GIF frames into texture atlas.");
            error->filePath = filePath;
        }
        return false;
    }

    QList<SpriteBox> boxes;
    boxes.reserve(packResult.frameRects.size());
    for (int i = 0; i < packResult.frameRects.size(); ++i) {
        SpriteBox sb;
        sb.rect = packResult.frameRects[i];
        sb.index = i;
        sb.selected = false;
        boxes.append(sb);
    }

    // Determine FPS
    int avgDelay = totalDelayMs / qMax(1, framePixmaps.size());
    int fps = (avgDelay > 0) ? qRound(1000.0 / avgDelay) : 12;
    if (fps <= 0) fps = 12;

    outDoc.setFilePath(filePath);
    outDoc.setAtlas(packResult.atlas);
    outDoc.setFrames(framePixmaps, boxes);

    QList<int> allIndices;
    allIndices.reserve(framePixmaps.size());
    for (int i = 0; i < framePixmaps.size(); ++i) {
        allIndices.append(i);
    }
    outDoc.setAnimation(QStringLiteral("default"), allIndices, fps, true);

    setProgress(100);
    setStatusMessage(tr("Extracted %1 GIF frames").arg(framePixmaps.size()));
    emit extractionFinished(framePixmaps.size());
    return true;
}
