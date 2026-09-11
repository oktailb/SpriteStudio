#include "extractor/godotextractor.h"
#include "packer/atlaspacker.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

GodotExtractor::GodotExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool GodotExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    if (fi.suffix().toLower() != QStringLiteral("tres")) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QString head = QString::fromUtf8(file.read(1024));
    file.close();
    return head.contains(QStringLiteral("SpriteFrames"));
}

bool GodotExtractor::read(const QString &filePath, SpriteDocument &doc, ExtractorError *error)
{
    setStatusMessage(tr("Reading Godot SpriteFrames resource..."));
    setProgress(10);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("Cannot open Godot resource file: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    if (!content.contains(QStringLiteral("SpriteFrames"))) {
        if (error) {
            error->code = ExtractorError::InvalidHeader;
            error->message = tr("File is not a valid Godot SpriteFrames resource: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    setProgress(25);

    // 1. Locate referenced atlas texture image
    QRegularExpression extRegex(QStringLiteral(R"re(\[ext_resource\s+[^\]]*path="([^"]+)"[^\]]*\])re"));
    QRegularExpressionMatch extMatch = extRegex.match(content);
    QString rawImagePath;
    if (extMatch.hasMatch()) {
        rawImagePath = extMatch.captured(1);
    }
    if (rawImagePath.startsWith(QStringLiteral("res://"))) {
        rawImagePath = rawImagePath.mid(6);
    }

    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    QString imagePath;

    if (!rawImagePath.isEmpty()) {
        if (dir.exists(rawImagePath)) {
            imagePath = dir.filePath(rawImagePath);
        } else {
            QString fname = QFileInfo(rawImagePath).fileName();
            if (dir.exists(fname)) {
                imagePath = dir.filePath(fname);
            }
        }
    }

    if (imagePath.isEmpty() || !QFile::exists(imagePath)) {
        QString candidate = dir.filePath(fileInfo.completeBaseName() + QStringLiteral(".png"));
        if (QFile::exists(candidate)) {
            imagePath = candidate;
        }
    }

    if (imagePath.isEmpty() || !QFile::exists(imagePath)) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Referenced texture atlas image not found for: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    setProgress(40);

    QImage atlasImg(imagePath);
    if (atlasImg.isNull()) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Failed to load texture atlas image: %1").arg(imagePath);
            error->filePath = imagePath;
        }
        return false;
    }

    setProgress(60);

    // 2. Parse sub_resources (AtlasTexture definitions)
    QRegularExpression subResRegex(QStringLiteral(
        R"re(\[sub_resource\s+type="AtlasTexture"\s+id="([^"]+)"\]\s*[\r\n]+(?:atlas\s*=\s*[^\r\n]+[\r\n]+)?region\s*=\s*Rect2\(\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*\))re"
    ));

    QMap<QString, int> subResToFrameIdx;
    QList<SpriteBox> boxes;
    QList<QPixmap> frames;

    QRegularExpressionMatchIterator iter = subResRegex.globalMatch(content);
    int frameIndex = 0;
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString subResId = match.captured(1);
        int rx = qRound(match.captured(2).toDouble());
        int ry = qRound(match.captured(3).toDouble());
        int rw = qRound(match.captured(4).toDouble());
        int rh = qRound(match.captured(5).toDouble());

        QRect boxRect(rx, ry, rw, rh);
        boxRect = boxRect.intersected(atlasImg.rect());
        if (boxRect.isEmpty()) continue;

        SpriteBox box;
        box.rect = boxRect;
        box.index = frameIndex;
        box.selected = false;
        boxes.append(box);

        QPixmap framePix = QPixmap::fromImage(atlasImg.copy(boxRect));
        frames.append(framePix);

        subResToFrameIdx.insert(subResId, frameIndex);
        frameIndex++;
    }

    setProgress(75);

    // 3. Parse animations block
    QMap<QString, SpriteAnimation> parsedAnimations;

    QRegularExpression animsListRegex(QStringLiteral(R"re("animations":\s*\[(.*)\]\s*\}\s*$)re"),
                                      QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch animsMatch = animsListRegex.match(content);

    if (animsMatch.hasMatch()) {
        QString animsArrayStr = animsMatch.captured(1);

        int braceDepth = 0;
        int blockStart = -1;
        QList<QString> animBlocks;

        for (int i = 0; i < animsArrayStr.length(); ++i) {
            QChar c = animsArrayStr.at(i);
            if (c == QLatin1Char('{')) {
                if (braceDepth == 0) {
                    blockStart = i;
                }
                braceDepth++;
            } else if (c == QLatin1Char('}')) {
                braceDepth--;
                if (braceDepth == 0 && blockStart != -1) {
                    animBlocks.append(animsArrayStr.mid(blockStart, i - blockStart + 1));
                    blockStart = -1;
                }
            }
        }

        QRegularExpression nameRegex(QStringLiteral(R"re("name":\s*(?:&?"([^"]+)"|([a-zA-Z0-9_]+)))re"));
        QRegularExpression speedRegex(QStringLiteral(R"re("speed":\s*([0-9.]+))re"));
        QRegularExpression loopRegex(QStringLiteral(R"re("loop":\s*(true|false))re"));
        QRegularExpression subResRefRegex(QStringLiteral(R"re(SubResource\("([^"]+)"\))re"));

        for (int b = 0; b < animBlocks.size(); ++b) {
            const QString &block = animBlocks[b];
            SpriteAnimation anim;
            anim.name = QStringLiteral("anim_%1").arg(b);
            anim.fps = 12;
            anim.loop = true;

            QRegularExpressionMatch nm = nameRegex.match(block);
            if (nm.hasMatch()) {
                anim.name = nm.captured(1).isEmpty() ? nm.captured(2) : nm.captured(1);
            }

            QRegularExpressionMatch sm = speedRegex.match(block);
            if (sm.hasMatch()) {
                anim.fps = qMax(1, qRound(sm.captured(1).toDouble()));
            }

            QRegularExpressionMatch lm = loopRegex.match(block);
            if (lm.hasMatch()) {
                anim.loop = (lm.captured(1) == QStringLiteral("true"));
            }

            QRegularExpressionMatchIterator fit = subResRefRegex.globalMatch(block);
            while (fit.hasNext()) {
                QRegularExpressionMatch fm = fit.next();
                QString refId = fm.captured(1);
                if (subResToFrameIdx.contains(refId)) {
                    anim.frameIndices.append(subResToFrameIdx.value(refId));
                }
            }

            if (!anim.frameIndices.isEmpty()) {
                parsedAnimations.insert(anim.name, anim);
            }
        }
    }

    if (parsedAnimations.isEmpty() && !frames.isEmpty()) {
        SpriteAnimation defAnim;
        defAnim.name = QStringLiteral("default");
        defAnim.fps = 10;
        defAnim.loop = true;
        for (int i = 0; i < frames.size(); ++i) {
            defAnim.frameIndices.append(i);
        }
        parsedAnimations.insert(defAnim.name, defAnim);
    }

    // 4. Update SpriteDocument directly
    doc.setFilePath(filePath);
    doc.setAtlas(atlasImg);
    doc.setFrames(frames, boxes);
    for (auto ait = parsedAnimations.begin(); ait != parsedAnimations.end(); ++ait) {
        doc.setAnimation(ait.key(), ait.value().frameIndices, ait.value().fps, ait.value().loop);
    }

    setProgress(100);
    setStatusMessage(tr("Extracted %1 frames and %2 animations from Godot resource.")
                         .arg(frames.size())
                         .arg(parsedAnimations.size()));
    emit extractionFinished(frames.size());
    return true;
}

bool GodotExtractor::write(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, ExtractorError *error)
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

    setStatusMessage(tr("Packing atlas for Godot..."));
    setProgress(20);

    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    QString baseName = fileInfo.completeBaseName();
    QString imageFilename = baseName + ".png";
    QString imagePath = dir.filePath(imageFilename);
    QString tresPath = dir.filePath(baseName + ".tres");

    AtlasPackResult packResult = AtlasPacker::pack(doc.frames(), 2);
    if (!packResult.success) {
        if (error) {
            error->code = ExtractorError::PackingFailed;
            error->message = tr("Failed to pack atlas frames for Godot export.");
            error->filePath = filePath;
        }
        return false;
    }

    setProgress(60);

    if (!packResult.atlas.save(imagePath, "PNG")) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to write Godot atlas image: %1").arg(imagePath);
            error->filePath = imagePath;
        }
        return false;
    }

    setProgress(80);

    QFile outFile(tresPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot write to Godot resource file: %1").arg(tresPath);
            error->filePath = tresPath;
        }
        return false;
    }

    QTextStream out(&outFile);

    // Write Godot 4.x SpriteFrames header
    out << "[gd_resource type=\"SpriteFrames\" load_steps=" << (packResult.frameRects.size() + 2) << " format=3]\n\n";
    out << "[ext_resource type=\"Texture2D\" path=\"res://" << imageFilename << "\" id=\"1_atlas\"]\n\n";

    for (int i = 0; i < packResult.frameRects.size(); ++i) {
        const QRect &r = packResult.frameRects[i];
        QString subResId = QString("AtlasTexture_%1").arg(i);
        out << "[sub_resource type=\"AtlasTexture\" id=\"" << subResId << "\"]\n";
        out << "atlas = ExtResource(\"1_atlas\")\n";
        out << "region = Rect2(" << r.x() << ", " << r.y() << ", " << r.width() << ", " << r.height() << ")\n\n";
    }

    out << "[resource]\n";
    out << "animations = [{\n";

    auto animations = doc.animations();
    if (animations.isEmpty()) {
        out << "\"frames\": [";
        for (int i = 0; i < packResult.frameRects.size(); ++i) {
            if (i > 0) out << ", ";
            out << "SubResource(\"AtlasTexture_" << i << "\")";
        }
        out << "],\n";
        out << "\"loop\": true,\n";
        out << "\"name\": &\"default\",\n";
        out << "\"speed\": 10.0\n";
        out << "}]\n";
    } else {
        bool firstAnim = true;
        for (auto it = animations.begin(); it != animations.end(); ++it) {
            if (!firstAnim) out << "}, {\n";
            firstAnim = false;

            out << "\"frames\": [";
            const QList<int> &fIndices = it.value().frameIndices;
            for (int j = 0; j < fIndices.size(); ++j) {
                if (j > 0) out << ", ";
                out << "SubResource(\"AtlasTexture_" << fIndices[j] << "\")";
            }
            out << "],\n";
            out << "\"loop\": " << (it.value().loop ? "true" : "false") << ",\n";
            out << "\"name\": &\"" << it.key() << "\",\n";
            out << "\"speed\": " << static_cast<double>(it.value().fps) << ".0\n";
        }
        out << "}]\n";
    }

    outFile.close();

    setProgress(100);
    setStatusMessage(tr("Exported Godot resource: %1 and image %2").arg(QFileInfo(tresPath).fileName(), imageFilename));
    return true;
}
