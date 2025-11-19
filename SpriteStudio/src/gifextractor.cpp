#include "gifextractor.h"
#include <QMovie>
#include <QDebug>
#include <QApplication>

GifExtractor::GifExtractor(QObject *parent) : Extractor(parent)
{
}

QList<QPixmap> GifExtractor::extractFrames(const QString &filePath)
{
    m_frames.clear();
    m_atlas_index.clear();

    QMovie movie(filePath);

    if (!movie.isValid()) {
        qWarning() << "Erreur: Le fichier n'est pas un GIF valide ou n'existe pas:" << filePath;
        return m_frames;
    }

    movie.setCacheMode(QMovie::CacheAll);
    if (!movie.jumpToNextFrame()) {
        qWarning() << "Erreur lors du saut à la première frame du GIF.";
        return m_frames;
    }
    auto nb_frames = movie.frameCount();
    int w = movie.frameRect().width();
    int h = movie.frameRect().height();
    int nb_cols = floor(sqrt(nb_frames));
    int nb_lines = floor(nb_frames / nb_cols);
    int carry = nb_frames % nb_cols;
    nb_lines += carry;
    m_atlas = QPixmap(w * nb_cols, h * nb_lines);

    QPainter painter(&m_atlas);
    int counter = 0;
    movie.start();

    for (int i = 0; i < nb_frames; ++i) {
        if (!movie.jumpToFrame(i)) {
            qWarning() << "Erreur lors du saut à la frame : " << i;
            break;
        }

        if (movie.state() != QMovie::Running) {
            movie.start();
        }

        QPixmap current = QPixmap::fromImage(movie.currentImage());
        this->addFrame(current);

        int line = floor(counter / nb_cols);
        int col = counter % nb_cols;

        painter.drawPixmap(col * w, line * h, current);
        m_atlas_index.push_back({col * w, line * h, w, h});

        counter++;
    }
    movie.stop();
    painter.end();

    emit extractionFinished(m_frames.size());
    return m_frames;
}
