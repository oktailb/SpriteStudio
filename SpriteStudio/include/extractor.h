#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <QObject>
#include <QPixmap>
#include <QList>
#include <QString>

class Extractor : public QObject
{
    Q_OBJECT
public:
    explicit Extractor(QObject *parent = nullptr) : QObject(parent)
    {
    }

    virtual QList<QPixmap> extractFrames(const QString &filePath) = 0;

    void addFrame(const QPixmap &pixmap)
    {
        if (!pixmap.isNull()) {
            m_frames.append(pixmap);
        }
    }

    QList<QPixmap> m_frames;

signals:
    void extractionFinished(int frameCount);

};

#endif // EXTRACTOR_H
