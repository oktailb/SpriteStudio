#ifndef GIFEXTRACTOR_H
#define GIFEXTRACTOR_H

#include "extractor/extractor.h"

/**
 * @brief Extractor derived class and plugin for Animated GIF sprites.
 */
class GifExtractor : public Extractor
{
    Q_OBJECT
public:
    explicit GifExtractor(QObject *parent = nullptr);
    explicit GifExtractor(QLabel * statusBar, QProgressBar * progressBar, QObject *parent = nullptr);

    // Plugin metadata
    QString id() const override { return QStringLiteral("gif_extractor"); }
    QString displayName() const override { return QStringLiteral("Animated GIF (*.gif)"); }
    QString description() const override { return QStringLiteral("Animated GIF format with frame sequence extraction."); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("gif") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport | SupportsAnimations;
    }
    bool canDecode(const QString &filePath) const override;

    QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance) override;
    QList<QPixmap> extractFromPixmap(int alphaThreshold, int verticalTolerance) override;
    bool           exportFrames(const QString &basePath, const QString &projectName, Extractor* in) override;
};

#endif // GIFEXTRACTOR_H
