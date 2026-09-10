#ifndef GODOTEXTRACTOR_H
#define GODOTEXTRACTOR_H

#include "extractor/extractor.h"

/**
 * @brief Extractor plugin for Godot Engine 4.x SpriteFrames (.tres) format.
 *
 * Exports native Godot 4 text resource (.tres) with embedded AtlasTexture regions
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
    QStringList supportedExtensions() const override {
        return { QStringLiteral("tres") };
    }

    Capabilities capabilities() const override {
        return CanImport | CanExport | SupportsAnimations | SupportsAtlasMetadata;
    }

    bool canDecode(const QString &filePath) const override;
    bool extract(const QString &filePath, SpriteDocument &doc, QString *errorMsg = nullptr) override;

    QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance) override;
    QList<QPixmap> extractFromPixmap(int alphaThreshold, int verticalTolerance) override;

    bool exportFrames(const QString &basePath, const QString &projectName, Extractor* in) override;
    bool exportDocument(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, QString *errorMsg = nullptr) override;
};

#endif // GODOTEXTRACTOR_H
