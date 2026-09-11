#ifndef GIFEXTRACTOR_H
#define GIFEXTRACTOR_H

#include "extractor/extractor.h"

/**
 * @brief Extractor plugin for Animated GIF sprites.
 */
class GifExtractor : public Extractor
{
    Q_OBJECT
public:
    explicit GifExtractor(QObject *parent = nullptr);
    ~GifExtractor() override = default;

    // Plugin metadata
    QString id() const override { return QStringLiteral("gif_extractor"); }
    QString displayName() const override { return QStringLiteral("Animated GIF"); }
    QString description() const override { return QStringLiteral("Animated GIF format with frame sequence extraction."); }
    QVersionNumber version() const override { return QVersionNumber(1, 1, 0); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("gif") };
    }
    Capabilities capabilities() const override {
        return CanImport | SupportsAnimations;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
};

#endif // GIFEXTRACTOR_H
