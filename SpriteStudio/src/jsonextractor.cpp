#include "jsonextractor.h"
#include <QDebug>
#include <QImage>
#include <QStack>
#include <QPoint>
#include <QRect>

JsonExtractor::JsonExtractor(QObject *parent) : Extractor(parent)
{
}

QList<QPixmap> JsonExtractor::extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance)
{
    m_frames.clear();
    m_atlas_index.clear();


    return m_frames;
}

bool JsonExtractor::exportFrames(const QString &basePath, const QString &projectName)
{
    return false;
}
