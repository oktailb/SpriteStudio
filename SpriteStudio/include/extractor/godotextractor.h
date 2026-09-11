#ifndef GODOTEXTRACTOR_H
#define GODOTEXTRACTOR_H

#include "extractor/extractor.h"

/**
 * @brief Extractor plugin for Godot Engine 4.x SpriteFrames (.tres) format.
 *
 * Imports and exports native Godot 4 text resource (.tres) with embedded AtlasTexture regions
 * and an accompanying PNG sprite sheet.
 */
class GodotExtractor : public Extractor
{
    Q_OBJECT

public:
    explicit GodotExtractor(QObject *parent = nullptr);
    ~GodotExtractor() override = default;

    QString id() const override { return QStringLiteral("godot_extractor"); }
    QString displayName() const override { return QStringLiteral("Godot Engine 4.x SpriteFrames"); }
    QString description() const override { return QStringLiteral("Native Godot 4 SpriteFrames resource for AnimatedSprite2D."); }
    QVersionNumber version() const override { return QVersionNumber(1, 1, 0); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("tres") };
    }

    Capabilities capabilities() const override {
        return CanImport | CanExport | SupportsAnimations | SupportsAtlasMetadata;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;
};

#endif // GODOTEXTRACTOR_H
