#include "extractor/jsonextractor.h"
#include "extractor/jsonExtractordialog.h"
#include "packer/atlaspacker.h"
#include "generated/version.h"
#include <QDebug>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <cmath>

JsonExtractor::JsonExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool JsonExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    if (fi.suffix().toLower() != QStringLiteral("json")) {
        return false;
    }

    QFile jsonFile(filePath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray head = jsonFile.read(2048);
    jsonFile.close();

    // Check for standard atlas JSON tags
    return head.contains("frames") || head.contains("meta");
}

static bool getJsonRoot(const QString &filePath, QJsonObject &dest, ExtractorError *error = nullptr)
{
    QFile jsonFile(filePath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = QObject::tr("Cannot open JSON file: %1").arg(jsonFile.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QByteArray jsonData = jsonFile.readAll();
    jsonFile.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        if (error) {
            error->code = ExtractorError::ParsingFailed;
            error->message = QObject::tr("JSON parse error: %1 at offset %2").arg(parseError.errorString()).arg(parseError.offset);
            error->filePath = filePath;
        }
        return false;
    }

    if (!doc.isObject()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = QObject::tr("JSON root must be an object.");
            error->filePath = filePath;
        }
        return false;
    }

    dest = doc.object();
    return true;
}

static QStringList findFilesGlob(const QString &path, const QString &filter)
{
    QStringList res;
    QDir dir(path);
    QStringList name_filters;
    name_filters << filter;
    QFileInfoList fil = dir.entryInfoList(name_filters, QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files);
    for (const QFileInfo &fi : fil) {
        if (fi.isFile()) res.push_back(fi.fileName());
    }
    return res;
}

bool JsonExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    setStatusMessage(tr("Reading JSON sprite atlas %1...").arg(QFileInfo(filePath).fileName()));
    setProgress(5);

    QJsonObject root;
    if (!getJsonRoot(filePath, root, error)) {
        return false;
    }

    // Locate companion atlas image
    QString imageFileName;
    if (root.contains("meta") && root["meta"].isObject()) {
        QJsonObject meta = root["meta"].toObject();
        if (meta.contains("image") && meta["image"].isString()) {
            imageFileName = meta["image"].toString();
        }
    }

    QFileInfo jsonFileInfo(filePath);
    QString imageFilePath;

    if (!imageFileName.isEmpty()) {
        imageFilePath = jsonFileInfo.dir().filePath(imageFileName);
        if (!QFile::exists(imageFilePath)) {
            imageFilePath = jsonFileInfo.dir().filePath(jsonFileInfo.completeBaseName() + ".png");
        }
    } else {
        imageFilePath = jsonFileInfo.dir().filePath(jsonFileInfo.completeBaseName() + ".png");
    }

    if (!QFile::exists(imageFilePath)) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Associated atlas image not found: %1").arg(imageFilePath);
            error->filePath = imageFilePath;
        }
        return false;
    }

    QImage atlasImage(imageFilePath);
    if (atlasImage.isNull()) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Failed to decode atlas image: %1").arg(imageFilePath);
            error->filePath = imageFilePath;
        }
        return false;
    }

    setProgress(30);

    QList<QPixmap> frames;
    QList<SpriteBox> boxes;
    QMap<QString, QList<int>> animationFrames;

    // Check for friend multi-file animations (e.g. project-walk.json)
    QFileInfo fileInfo(imageFilePath);
    QStringList friendAnimations = findFilesGlob(fileInfo.absolutePath(), fileInfo.baseName() + "-*.json");
    if (friendAnimations.isEmpty()) {
        friendAnimations.push_back(jsonFileInfo.fileName());
    }

    for (const QString &currentAnimation : friendAnimations) {
        QString subJsonPath = fileInfo.absolutePath() + QDir::separator() + currentAnimation;
        QJsonObject currentRoot;
        if (!getJsonRoot(subJsonPath, currentRoot, nullptr)) {
            continue;
        }

        if (currentRoot.contains("frames")) {
            if (currentRoot["frames"].isObject()) {
                extractFromTexturePackerFormat(currentRoot["frames"].toObject(), atlasImage, frames, boxes, animationFrames);
            } else if (currentRoot["frames"].isArray()) {
                extractFromArrayFormat(currentRoot["frames"].toArray(), atlasImage, frames, boxes, animationFrames);
            }
        }

        if (currentRoot.contains("meta") && currentRoot["meta"].isObject()) {
            QJsonObject meta = currentRoot["meta"].toObject();
            if (meta.contains("frameTags") && meta["frameTags"].isArray()) {
                QMap<QString, QList<int>> tagAnimations;
                extractAnimationsFromFrameTags(meta["frameTags"].toArray(), tagAnimations, frames.size());
                if (!tagAnimations.isEmpty()) {
                    animationFrames = tagAnimations;
                }
            }
        }
    }

    if (frames.isEmpty()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("No frames could be extracted from JSON: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    // Default animation fallback if none parsed
    QMap<QString, SpriteAnimation> parsedAnimations;
    if (animationFrames.isEmpty()) {
        SpriteAnimation defAnim;
        defAnim.name = jsonFileInfo.completeBaseName();
        defAnim.fps = 12;
        defAnim.loop = true;
        for (int i = 0; i < frames.size(); ++i) {
            defAnim.frameIndices.append(i);
        }
        parsedAnimations.insert(defAnim.name, defAnim);
    } else {
        for (auto it = animationFrames.begin(); it != animationFrames.end(); ++it) {
            SpriteAnimation anim;
            anim.name = it.key();
            anim.frameIndices = it.value();
            anim.fps = 12;
            anim.loop = true;
            parsedAnimations.insert(anim.name, anim);
        }
    }

    // Populate SpriteDocument directly
    outDoc.setFilePath(filePath);
    outDoc.setAtlas(atlasImage);
    outDoc.setFrames(frames, boxes);
    for (auto ait = parsedAnimations.begin(); ait != parsedAnimations.end(); ++ait) {
        outDoc.setAnimation(ait.key(), ait.value().frameIndices, ait.value().fps, ait.value().loop);
    }

    setProgress(100);
    setStatusMessage(tr("Imported %1 frames, %2 animations from JSON").arg(frames.size()).arg(parsedAnimations.size()));
    emit extractionFinished(frames.size());
    return true;
}

void JsonExtractor::extractFromTexturePackerFormat(const QJsonObject &framesObj,
                                                  const QImage &atlasImage,
                                                  QList<QPixmap> &frames,
                                                  QList<SpriteBox> &boxes,
                                                  QMap<QString, QList<int>> &animationFrames)
{
    int baseIndex = frames.size();

    for (auto it = framesObj.begin(); it != framesObj.end(); ++it) {
        QString frameName = it.key();
        QJsonValue frameValue = it.value();
        if (!frameValue.isObject()) continue;

        QJsonObject frameObj = frameValue.toObject();
        if (!frameObj.contains("frame") || !frameObj["frame"].isObject()) continue;

        QJsonObject frameRect = frameObj["frame"].toObject();
        int x = frameRect.value("x").toInt();
        int y = frameRect.value("y").toInt();
        int w = frameRect.value("w").toInt();
        int h = frameRect.value("h").toInt();

        if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
            x + w > atlasImage.width() || y + h > atlasImage.height()) {
            continue;
        }

        QPixmap framePix = QPixmap::fromImage(atlasImage.copy(x, y, w, h));
        int currentIndex = baseIndex + frames.size();

        SpriteBox box;
        box.rect = QRect(x, y, w, h);
        box.selected = false;
        box.index = currentIndex;

        frames.append(framePix);
        boxes.append(box);

        QString animName = extractAnimationName(frameName);
        if (!animName.isEmpty()) {
            animationFrames[animName].append(currentIndex);
        }
    }
}

void JsonExtractor::extractFromArrayFormat(const QJsonArray &framesArray,
                                          const QImage &atlasImage,
                                          QList<QPixmap> &frames,
                                          QList<SpriteBox> &boxes,
                                          QMap<QString, QList<int>> &animationFrames)
{
    int baseIndex = frames.size();

    for (int i = 0; i < framesArray.size(); ++i) {
        QJsonValue frameValue = framesArray[i];
        if (!frameValue.isObject()) continue;

        QJsonObject frameObj = frameValue.toObject();
        QJsonObject frameRect;
        if (frameObj.contains("frame") && frameObj["frame"].isObject()) {
            frameRect = frameObj["frame"].toObject();
        } else if (frameObj.contains("x") && frameObj.contains("y") &&
                   frameObj.contains("w") && frameObj.contains("h")) {
            frameRect = frameObj;
        } else {
            continue;
        }

        int x = frameRect.value("x").toInt();
        int y = frameRect.value("y").toInt();
        int w = frameRect.value("w").toInt();
        int h = frameRect.value("h").toInt();

        if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
            x + w > atlasImage.width() || y + h > atlasImage.height()) {
            continue;
        }

        QPixmap framePix = QPixmap::fromImage(atlasImage.copy(x, y, w, h));
        int currentIndex = baseIndex + frames.size();

        SpriteBox box;
        box.rect = QRect(x, y, w, h);
        box.selected = false;
        box.index = currentIndex;

        frames.append(framePix);
        boxes.append(box);

        if (frameObj.contains("filename") && frameObj["filename"].isString()) {
            QString filename = frameObj["filename"].toString();
            QString animName = extractAnimationName(filename);
            if (!animName.isEmpty()) {
                animationFrames[animName].append(currentIndex);
            }
        }
    }
}

void JsonExtractor::extractAnimationsFromFrameTags(const QJsonArray &frameTagsArray,
                                                  QMap<QString, QList<int>> &animationFrames,
                                                  int totalFrames)
{
    for (const QJsonValue &tagValue : frameTagsArray) {
        if (!tagValue.isObject()) continue;

        QJsonObject tagObj = tagValue.toObject();
        if (!tagObj.contains("name") || !tagObj["name"].isString() ||
            !tagObj.contains("from") || !tagObj.contains("to")) {
            continue;
        }

        QString animName = tagObj["name"].toString();
        int from = tagObj["from"].toInt();
        int to = tagObj["to"].toInt();

        QList<int> frameSeq;
        for (int i = from; i <= to; ++i) {
            if (i >= 0 && i < totalFrames) {
                frameSeq.append(i);
            }
        }

        if (!frameSeq.isEmpty()) {
            animationFrames[animName] = frameSeq;
        }
    }
}

QString JsonExtractor::extractAnimationName(const QString &frameName)
{
    QRegularExpression regex(QStringLiteral(R"re(^([a-zA-Z0-9_-]+)[_/]\d+$)re"));
    QRegularExpressionMatch match = regex.match(frameName);
    if (match.hasMatch()) {
        QString animName = match.captured(1);
        if (animName.toLower() != QStringLiteral("frame")) {
            return animName;
        }
    }
    return QString();
}

bool JsonExtractor::write(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(options);

    if (doc.frameCount() == 0) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("No frames in document to export.");
            error->filePath = filePath;
        }
        return false;
    }

    QFileInfo fi(filePath);
    QString baseName = fi.completeBaseName();
    QDir dir = fi.dir();
    QString pngFileName = baseName + QStringLiteral(".png");
    QString pngFilePath = dir.filePath(pngFileName);

    setStatusMessage(tr("Packing atlas for JSON export..."));
    setProgress(20);

    AtlasPackResult packResult = AtlasPacker::pack(doc.frames(), 2);
    if (!packResult.success) {
        if (error) {
            error->code = ExtractorError::PackingFailed;
            error->message = tr("Failed to pack frames for JSON export.");
            error->filePath = filePath;
        }
        return false;
    }

    setProgress(50);

    if (!packResult.atlas.save(pngFilePath, "PNG")) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to save companion image: %1").arg(pngFilePath);
            error->filePath = pngFilePath;
        }
        return false;
    }

    setProgress(75);

    // Build TexturePacker compatible JSON
    QJsonObject rootObj;
    QJsonObject framesObj;

    for (int i = 0; i < packResult.frameRects.size(); ++i) {
        const QRect &r = packResult.frameRects[i];
        QJsonObject frameData;

        QJsonObject fRect;
        fRect["x"] = r.x();
        fRect["y"] = r.y();
        fRect["w"] = r.width();
        fRect["h"] = r.height();
        frameData["frame"] = fRect;
        frameData["rotated"] = false;
        frameData["trimmed"] = false;

        QJsonObject sRect;
        sRect["x"] = 0;
        sRect["y"] = 0;
        sRect["w"] = r.width();
        sRect["h"] = r.height();
        frameData["spriteSourceSize"] = sRect;

        QJsonObject srcSize;
        srcSize["w"] = r.width();
        srcSize["h"] = r.height();
        frameData["sourceSize"] = srcSize;

        QString frameKey = QStringLiteral("%1_%2").arg(baseName).arg(i, 4, 10, QLatin1Char('0'));
        framesObj[frameKey] = frameData;
    }
    rootObj["frames"] = framesObj;

    // Meta object
    QJsonObject metaObj;
    metaObj["app"] = QStringLiteral("SpriteStudio");
    metaObj["version"] = QString(PROJECT_VERSION);
    metaObj["image"] = pngFileName;
    metaObj["format"] = QStringLiteral("RGBA8888");

    QJsonObject sizeObj;
    sizeObj["w"] = packResult.dimensions.width();
    sizeObj["h"] = packResult.dimensions.height();
    metaObj["size"] = sizeObj;
    metaObj["scale"] = QStringLiteral("1");

    // Frame tags for animations
    QJsonArray frameTagsArray;
    for (auto it = doc.animations().begin(); it != doc.animations().end(); ++it) {
        QJsonObject tagObj;
        tagObj["name"] = it.key();
        if (!it.value().frameIndices.isEmpty()) {
            tagObj["from"] = it.value().frameIndices.first();
            tagObj["to"] = it.value().frameIndices.last();
        } else {
            tagObj["from"] = 0;
            tagObj["to"] = 0;
        }
        tagObj["direction"] = QStringLiteral("forward");
        tagObj["fps"] = it.value().fps;
        frameTagsArray.append(tagObj);
    }
    metaObj["frameTags"] = frameTagsArray;

    rootObj["meta"] = metaObj;

    QFile jsonOut(filePath);
    if (!jsonOut.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot write to JSON file: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QJsonDocument jsonDoc(rootObj);
    jsonOut.write(jsonDoc.toJson(QJsonDocument::Indented));
    jsonOut.close();

    setProgress(100);
    setStatusMessage(tr("Exported JSON descriptor %1 and image %2").arg(QFileInfo(filePath).fileName(), pngFileName));
    return true;
}
