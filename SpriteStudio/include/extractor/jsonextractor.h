#ifndef JSONEXTRACTOR_H
#define JSONEXTRACTOR_H

#include "extractor.h"
#include <QLabel>
#include <QProgressBar>

/**
 * @brief Extractor derivated class for JSON sprite databases used in industry.
 *
 */
class JsonExtractor : public Extractor
{
    Q_OBJECT
public:
    explicit JsonExtractor(QLabel * statusBar, QProgressBar * progressBar, QObject *parent = nullptr);

    QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance) override;
    QList<QPixmap> extractFromPixmap(int alphaThreshold, int verticalTolerance) override;
    bool           exportFrames(const QString &basePath, const QString &projectName, Extractor* in) override;
private:
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
};

#endif // JSONEXTRACTOR_H
