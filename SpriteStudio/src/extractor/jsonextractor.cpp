#include "extractor/jsonextractor.h"
#include "extractor/jsonExtractordialog.h"
#include "ui_jsonExtractordialog.h"
#include "extractor/export.h"
#include <QDebug>
#include <QImage>
#include <QStack>
#include <QPoint>
#include <QRect>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

JsonExtractor::JsonExtractor(QObject *parent) : Extractor(parent)
{
}

QList<QPixmap> JsonExtractor::extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance)
{
    // The JsonExtractor is primarily an EXPORTER, not an importer.
    // This method is a required implementation but serves as a no-op here.
    Q_UNUSED(alphaThreshold);
    Q_UNUSED(verticalTolerance);

    m_frames.clear();
    m_atlas_index.clear();
    m_filePath = filePath;
    return m_frames;
}

QList<QPixmap> JsonExtractor::extractFromPixmap(int alphaThreshold, int verticalTolerance)
{
    // Implementation of the pure virtual method.
    // JsonExtractor does not support re-extraction based on alpha/tolerance.
    Q_UNUSED(alphaThreshold);
    Q_UNUSED(verticalTolerance);

    return m_frames;
}

QJsonDocument *JsonExtractor::exportToTexturePacker(QString projectName,
                                     const ExportOptions &opts,
                                     const QString anim,
                                     Extractor * in)
{
    QList<int> frameIndices = in->m_animationsData[anim].frameIndices;

    QJsonObject framesDict;

    for (int i = 0; i < frameIndices.size(); ++i) {
        int frameIdx = frameIndices[i];

        if (frameIdx < 0 || frameIdx >= in->m_atlas_index.size()) {
            qWarning() << "Frame index out of bounds:" << frameIdx;
            continue;
        }

        const Extractor::Box& realBox = in->m_atlas_index[frameIdx];

        QJsonObject frameData;
        frameData["filename"] = QString("%1_%2").arg(anim).arg(i, 4, 10, QChar('0'));

        QJsonObject frameRect;
        frameRect["x"] = realBox.rect.x();
        frameRect["y"] = realBox.rect.y();
        frameRect["w"] = realBox.rect.width();
        frameRect["h"] = realBox.rect.height();

        frameData["frame"] = frameRect;
        frameData["rotated"] = false;
        frameData["trimmed"] = opts.trimSprites;

        frameData["spriteSourceSize"] = frameRect;

        QJsonObject sourceSize;
        sourceSize["w"] = realBox.rect.width();
        sourceSize["h"] = realBox.rect.height();
        frameData["sourceSize"] = sourceSize;

        QString frameKey = QString("%1_%2").arg(anim).arg(i, 4, 10, QChar('0'));
        framesDict[frameKey] = frameData;
    }

    QJsonObject root;
    root["frames"] = framesDict;

    QJsonObject meta;
    meta["app"] = "Sprite Studio";
    meta["version"] = "1.0";
    meta["image"] = projectName + ".png";
    meta["format"] = "RGBA8888";
    meta["size"] = QJsonObject{
        {"w", in->m_atlas.width()},
        {"h", in->m_atlas.height()}
    };
    meta["scale"] = "1";

    if (opts.embedAnimations) {
        QJsonArray frameTags;
        QJsonObject animTag;
        animTag["name"] = anim;
        animTag["from"] = 0;
        animTag["to"] = frameIndices.size() - 1;
        animTag["direction"] = "forward";
        frameTags.append(animTag);
        meta["frameTags"] = frameTags;
    }

    root["meta"] = meta;

    return new QJsonDocument(root);
}

bool JsonExtractor::exportFrames(const QString &basePath, const QString &projectName, Extractor *in)
{
    jsonExtractorDialog* dialog = new jsonExtractorDialog(in, projectName);
    dialog->exec();

    ExportOptions opts = dialog->getOpts();

    QList<QString> animationsNames = dialog->selectedAnimations();

    for (const QString &anim : animationsNames) {

        QJsonDocument * doc = nullptr;

        switch (dialog->selectedFormat())
        {
        case Format::FORMAT_TEXTUREPACKER_JSON: { doc = exportToTexturePacker(projectName, opts, anim, in); break; }
        case Format::FORMAT_PHASER_JSON: { doc = exportToTexturePacker(projectName, opts, anim, in); break; }
        case Format::FORMAT_ASEPRITE_JSON: { doc = exportToTexturePacker(projectName, opts, anim, in); break; }
        default: {
            qDebug() << tr("_selected_format_error");
            return false;
        }
        }


        QString jsonFilePath = QDir(basePath).filePath(projectName + "-" + anim + ".json");

        QFile jsonFile(jsonFilePath);
        if (jsonFile.open(QIODevice::WriteOnly)) {
            // Write JSON data to file in an indented (human-readable) format.
            jsonFile.write(doc->toJson(QJsonDocument::Indented));
            jsonFile.close();

            qDebug() << tr("_export_success") << tr("_export_atlas_success") << basePath;
        } else {
            qDebug() << tr("_write_error") << tr("_json_permissions");
            delete doc;
            return false;
        }
        delete doc;
    }

    if (dialog->replaceAtlas()) {
        QString pngFilePath = QDir(basePath).filePath(projectName + ".png");

        if (!in->m_atlas.save(pngFilePath, "PNG")) {
            qDebug() << tr("_write_error") << ": " <<  tr("_png_permissions");
            return false;
        }
    }
    return true; // Export completed successfully.
}
