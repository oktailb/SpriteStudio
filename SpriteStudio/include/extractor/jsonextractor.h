#ifndef JSONEXTRACTOR_H
#define JSONEXTRACTOR_H

#include "extractor/extractor.h"
#include <QLabel>
#include <QProgressBar>
#include "extractor/jsonExtractordialog.h"

/**
 * @brief Extractor derived class and plugin for JSON sprite databases (TexturePacker, Aseprite).
 */
class JsonExtractor : public Extractor
{
    Q_OBJECT
public:
    explicit JsonExtractor(QObject *parent = nullptr);
    explicit JsonExtractor(QLabel * statusBar, QProgressBar * progressBar, QObject *parent = nullptr);

    // Plugin metadata
    QString id() const override { return QStringLiteral("json_extractor"); }
    QString displayName() const override { return QStringLiteral("JSON Atlas"); }
    QString description() const override { return QStringLiteral("TexturePacker and Aseprite JSON atlas descriptor with image."); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("json") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport | SupportsAnimations | SupportsAtlasMetadata;
    }
    bool canDecode(const QString &filePath) const override;

    QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance) override;
    QList<QPixmap> extractFromPixmap(int alphaThreshold, int verticalTolerance) override;
    bool           exportFrames(const QString &basePath, const QString &projectName, Extractor* in) override;

private:
    jsonExtractorDialog* dialog = nullptr;

    QJsonDocument * exportToTexturePacker(QString projectName,
                                          const ExportOptions &opts,
                                          const QString anim, const QString format,
                                          Extractor * in);
    void            extractFromTexturePackerFormat(const QJsonObject& framesObj,
                                                   QMap<QString, QList<int>>& animationFrames);
    void            extractFromArrayFormat(const QJsonArray& framesArray,
                                           QMap<QString, QList<int>>& animationFrames);
    void            extractAnimationsFromFrameTags(const QJsonArray& frameTagsArray,
                                        QMap<QString, QList<int>>& animationFrames, int shift);
    QString         extractAnimationName(const QString& frameName);
    void            generatePackedAtlas(Extractor *in);
    void            generateIndividualAtlas(Extractor *in, QString basePath, QString projectName);
};

#endif // JSONEXTRACTOR_H
