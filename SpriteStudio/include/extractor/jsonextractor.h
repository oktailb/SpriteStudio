#ifndef JSONEXTRACTOR_H
#define JSONEXTRACTOR_H

#include "extractor.h"

/**
 * @brief Extractor derivated class for JSON sprite databases used in industry.
 *
 */
class JsonExtractor : public Extractor
{
    Q_OBJECT
public:
    explicit JsonExtractor(QObject *parent = nullptr);

    QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance) override;
    QList<QPixmap> extractFromPixmap(int alphaThreshold, int verticalTolerance) override;
    bool           exportFrames(const QString &basePath, const QString &projectName, Extractor* in) override;
private:
    QJsonDocument *exportToTexturePacker(QString projectName,
                                         const ExportOptions &opts,
                                         const QString anim,
                                         Extractor * in);
};

#endif // JSONEXTRACTOR_H
