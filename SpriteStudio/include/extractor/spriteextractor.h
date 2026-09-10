#ifndef SPRITEEXTRACTOR_H
#define SPRITEEXTRACTOR_H

#include "extractor/extractor.h"

/**
 * @brief Extractor derived class and plugin for static sprite sheets (PNG, JPG, JPEG, BMP).
 */
class SpriteExtractor : public Extractor
{
    Q_OBJECT
public:
    explicit SpriteExtractor(QObject *parent = nullptr);

    // Plugin metadata
    QString id() const override { return QStringLiteral("sprite_extractor"); }
    QString displayName() const override { return QStringLiteral("Sprite Sheet"); }
    QString description() const override { return QStringLiteral("Static sprite sheet with automatic alpha edge detection."); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport | SupportsAtlasMetadata;
    }
    bool canDecode(const QString &filePath) const override;

    QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance) override;
    QList<QPixmap> extractFromPixmap(int alphaThreshold, int verticalTolerance) override;
    bool           exportFrames(const QString &basePath, const QString &projectName, Extractor* in) override;
};

#endif // SPRITEEXTRACTOR_H
