#ifndef GIFEXTRACTOR_H
#define GIFEXTRACTOR_H

#include "extractor/extractor.h"

/**
 * @brief Extractor derivated class for Animated GIF sprites.
 *
 */
class GifExtractor : public Extractor
{
  Q_OBJECT
public:
  explicit GifExtractor(QLabel * statusBar, QProgressBar * progressBar, QObject *parent = nullptr);

    QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance) override;
    QList<QPixmap> extractFromPixmap(int alphaThreshold, int verticalTolerance) override;
    bool           exportFrames(const QString &basePath, const QString &projectName, Extractor* in) override;
};

#endif // GIFEXTRACTOR_H
