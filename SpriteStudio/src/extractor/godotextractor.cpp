#include "extractor/godotextractor.h"
#include "model/spritedocument.h"
#include "packer/atlaspacker.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

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
    QByteArray header = file.read(1024);
    file.close();
    return header.contains("SpriteFrames");
}

bool GodotExtractor::extract(const QString &filePath, SpriteDocument &doc, QString *errorMsg)
{
    setStatusMessage(tr("Importing Godot SpriteFrames..."));
    setProgress(10);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMsg) *errorMsg = tr("Cannot open Godot resource file: %1").arg(filePath);
        return false;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    if (!content.contains(QStringLiteral("SpriteFrames"))) {
        if (errorMsg) *errorMsg = tr("File is not a valid Godot SpriteFrames resource: %1").arg(filePath);
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
        if (errorMsg) *errorMsg = tr("Referenced texture atlas image not found for: %1").arg(filePath);
        return false;
    }

    setProgress(40);

    QImage atlasImg(imagePath);
    if (atlasImg.isNull()) {
        if (errorMsg) *errorMsg = tr("Failed to load texture atlas image: %1").arg(imagePath);
        return false;
    }
    m_atlas = atlasImg;

    setProgress(60);

    // 2. Parse sub_resources (AtlasTexture definitions)
    QMap<QString, int> subResToFrameIdx;
    QList<QPixmap> frames;
    QList<SpriteBox> boxes;

    QRegularExpression sectionHeaderRegex(QStringLiteral(R"re(\[sub_resource\s+type="AtlasTexture"\s+id="([^"]+)"\])re"));
    QRegularExpressionMatchIterator it = sectionHeaderRegex.globalMatch(content);

    struct SubResInfo {
        QString id;
        int startPos;
        int endPos;
    };
    QList<SubResInfo> subSections;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        SubResInfo info;
        info.id = m.captured(1);
        info.startPos = m.capturedEnd();
        info.endPos = content.length();
        if (!subSections.isEmpty()) {
            subSections.last().endPos = m.capturedStart();
        }
        subSections.append(info);
    }

    if (!subSections.isEmpty()) {
        int nextBracket = content.indexOf(QLatin1Char('['), subSections.last().startPos);
        if (nextBracket != -1) {
            subSections.last().endPos = nextBracket;
        }
    }

    QRegularExpression rectRegex(QStringLiteral(R"re(region\s*=\s*Rect2\s*\(\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)\s*\))re"));

    for (const SubResInfo &info : subSections) {
        QString sectionBody = content.mid(info.startPos, info.endPos - info.startPos);
        QRegularExpressionMatch rm = rectRegex.match(sectionBody);
        if (rm.hasMatch()) {
            int rx = qRound(rm.captured(1).toDouble());
            int ry = qRound(rm.captured(2).toDouble());
            int rw = qRound(rm.captured(3).toDouble());
            int rh = qRound(rm.captured(4).toDouble());

            QRect r(rx, ry, rw, rh);
            r = r.intersected(m_atlas.rect());
            if (r.width() > 0 && r.height() > 0) {
                QPixmap frame = QPixmap::fromImage(m_atlas.copy(r));
                int frameIdx = frames.size();
                frames.append(frame);

                SpriteBox sb;
                sb.rect = r;
                sb.index = frameIdx;
                sb.selected = false;
                boxes.append(sb);

                subResToFrameIdx.insert(info.id, frameIdx);
            }
        }
    }

    if (frames.isEmpty()) {
        if (errorMsg) *errorMsg = tr("No valid AtlasTexture regions found in: %1").arg(filePath);
        return false;
    }

    setProgress(80);

    // 3. Parse animations block inside [resource]
    QMap<QString, SpriteAnimation> parsedAnimations;
    int animStart = content.indexOf(QStringLiteral("animations = ["));
    if (animStart != -1) {
        animStart += 14; // length of "animations = ["
        int bracketDepth = 1;
        int animEnd = animStart;
        for (int i = animStart; i < content.length(); ++i) {
            if (content.at(i) == QLatin1Char('[')) bracketDepth++;
            else if (content.at(i) == QLatin1Char(']')) {
                bracketDepth--;
                if (bracketDepth == 0) {
                    animEnd = i;
                    break;
                }
            }
        }

        QString animsArrayStr = content.mid(animStart, animEnd - animStart);

        // Find each animation dictionary {...} respecting nested braces
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

    // Default animation fallback if none parsed
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

    // 4. Update SpriteDocument
    doc.setFilePath(filePath);
    doc.setAtlas(m_atlas);
    doc.setFrames(frames, boxes);
    for (auto ait = parsedAnimations.begin(); ait != parsedAnimations.end(); ++ait) {
        doc.setAnimation(ait.key(), ait.value().frameIndices, ait.value().fps, ait.value().loop);
    }

    // 5. Synchronize local extractor state
    syncFromDocument(doc);

    setProgress(100);
    setStatusMessage(tr("Imported Godot resource: %1 (%2 frames)").arg(fileInfo.fileName()).arg(frames.size()));
    emit extractionFinished(frames.size());
    return true;
}

QList<QPixmap> GodotExtractor::extractFrames(const QString &filePath, int alphaThreshold, int verticalTolerance)
{
    Q_UNUSED(alphaThreshold);
    Q_UNUSED(verticalTolerance);
    SpriteDocument doc;
    QString errorMsg;
    if (extract(filePath, doc, &errorMsg)) {
        return m_frames;
    }
    return {};
}

QList<QPixmap> GodotExtractor::extractFromPixmap(int alphaThreshold, int verticalTolerance)
{
    Q_UNUSED(alphaThreshold);
    Q_UNUSED(verticalTolerance);
    return m_frames;
}

bool GodotExtractor::exportFrames(const QString &basePath, const QString &projectName, Extractor* in)
{
    if (!in || in->m_frames.isEmpty()) return false;
    SpriteDocument doc;
    in->syncToDocument(doc);
    QString filePath = QDir(basePath).filePath(projectName + ".tres");
    ExportOptions opts;
    return exportDocument(filePath, doc, opts);
}

bool GodotExtractor::exportDocument(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, QString *errorMsg)
{
    Q_UNUSED(options);

    if (doc.frameCount() == 0) {
        if (errorMsg) *errorMsg = QStringLiteral("No frames in document to export.");
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
        if (errorMsg) *errorMsg = QStringLiteral("Failed to pack atlas frames for Godot export.");
        return false;
    }

    setProgress(60);

    if (!packResult.atlas.save(imagePath, "PNG")) {
        if (errorMsg) *errorMsg = QString("Failed to write Godot atlas image: %1").arg(imagePath);
        return false;
    }

    setProgress(80);

    QFile outFile(tresPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg) *errorMsg = QString("Cannot write to Godot resource file: %1").arg(tresPath);
        return false;
    }

    QTextStream out(&outFile);

    int subResourceCount = doc.frameCount();
    out << "[gd_resource type=\"SpriteFrames\" load_steps=" << (subResourceCount + 2) << " format=3]\n\n";
    out << "[ext_resource type=\"Texture2D\" path=\"res://" << imageFilename << "\" id=\"1_atlas\"]\n\n";

    for (int i = 0; i < doc.frameCount(); ++i) {
        const QRect &r = packResult.frameRects[i];
        out << "[sub_resource type=\"AtlasTexture\" id=\"AtlasTexture_" << i << "\"]\n";
        out << "atlas = ExtResource(\"1_atlas\")\n";
        out << "region = Rect2(" << r.x() << ", " << r.y() << ", " << r.width() << ", " << r.height() << ")\n\n";
    }

    out << "[resource]\n";
    out << "animations = [";

    QMap<QString, SpriteAnimation> anims = doc.animations();
    if (anims.isEmpty()) {
        SpriteAnimation defaultAnim;
        defaultAnim.name = QStringLiteral("default");
        defaultAnim.fps = 10;
        defaultAnim.loop = true;
        for (int i = 0; i < doc.frameCount(); ++i) {
            defaultAnim.frameIndices.append(i);
        }
        anims.insert(defaultAnim.name, defaultAnim);
    }

    bool firstAnim = true;
    for (auto it = anims.begin(); it != anims.end(); ++it) {
        if (!firstAnim) out << ", ";
        firstAnim = false;

        out << "{\n";
        out << "\"frames\": [";

        bool firstFrame = true;
        for (int frameIdx : it.value().frameIndices) {
            if (frameIdx >= 0 && frameIdx < doc.frameCount()) {
                if (!firstFrame) out << ", ";
                firstFrame = false;

                out << "{\n";
                out << "\"duration\": 1.0,\n";
                out << "\"texture\": SubResource(\"AtlasTexture_" << frameIdx << "\")\n";
                out << "}";
            }
        }

        out << "],\n";
        out << "\"loop\": " << (it.value().loop ? "true" : "false") << ",\n";
        out << "\"name\": &\"" << it.key() << "\",\n";
        out << "\"speed\": " << static_cast<double>(it.value().fps) << "\n";
        out << "}";
    }

    out << "]\n";
    outFile.close();

    setProgress(100);
    setStatusMessage(tr("Exported Godot resource: %1 and image %2").arg(QFileInfo(tresPath).fileName(), imageFilename));
    return true;
}
