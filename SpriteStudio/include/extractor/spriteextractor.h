#ifndef SPRITEEXTRACTOR_H
#define SPRITEEXTRACTOR_H

#include "extractor/extractor.h"

/**
 * @brief Extractor plugin for static sprite sheets (PNG, JPG, JPEG, BMP).
 */
class SpriteExtractor : public Extractor
{
    Q_OBJECT

public:
    explicit SpriteExtractor(QObject *parent = nullptr);
    ~SpriteExtractor() override = default;

    // Plugin metadata
    QString id() const override { return QStringLiteral("sprite_extractor"); }
    QString displayName() const override { return QStringLiteral("Sprite Sheet"); }
    QString description() const override { return QStringLiteral("Static sprite sheet with automatic alpha edge detection."); }
    QVersionNumber version() const override { return QVersionNumber(1, 1, 0); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport | SupportsAtlasMetadata;
    }

    bool canDecode(const QString &filePath) const override;

    // Core Codec API
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;

    // Segmentation engine on arbitrary image
    bool extractFromImage(const QImage &image, SpriteDocument &outDoc, const SpriteSheetOptions &options = SpriteSheetOptions());
    bool extractFromImage(const QImage &image, SpriteDocument &outDoc, int alphaThreshold, int verticalTolerance);
    bool extractToImages(const QImage &sourceImage,
                         QList<QImage> &outFrames,
                         QList<SpriteBox> &outBoxes,
                         const SpriteSheetOptions &options = SpriteSheetOptions());

    // Options configuration
    void setOptions(const SpriteSheetOptions &options) { m_options = options; }
    const SpriteSheetOptions& options() const { return m_options; }
    SpriteSheetOptions& options() { return m_options; }
    void setSmartCropEnabled(bool enabled) { m_options.smartCrop = enabled; }
    void setOverlapThreshold(double threshold) { m_options.overlapThreshold = threshold; }

private:
    SpriteSheetOptions m_options;
};

#endif // SPRITEEXTRACTOR_H
