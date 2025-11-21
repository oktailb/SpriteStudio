#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <QObject>
#include <QPixmap>
#include <QList>
#include <QString>
#include <qpainter.h>

class Extractor : public QObject
{
    Q_OBJECT
public:
    explicit Extractor(QObject *parent = nullptr) : QObject(parent)
    {
    }

    virtual QList<QPixmap> extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance) = 0;
    virtual bool           exportFrames(const QString &basePath, const QString &projectName) = 0;

    void addFrame(const QPixmap &pixmap)
    {
        if (!pixmap.isNull()) {
            m_frames.append(pixmap);
        }
    }
    struct Box {
        int x;
        int y;
        int w;
        int h;
    };

    QList<QPixmap> m_frames;
    QPixmap m_atlas;
    QList<Box> m_atlas_index;

signals:
    void extractionFinished(int frameCount);

};

#endif // EXTRACTOR_H
