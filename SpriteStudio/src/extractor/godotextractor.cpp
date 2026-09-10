#include "extractor/godotextractor.h"
#include "model/spritedocument.h"
#include "packer/atlaspacker.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>

GodotExtractor::GodotExtractor(QObject *parent)
    : Extractor(parent)
{
}

GodotExtractor::GodotExtractor(QLabel *statusBar, QProgressBar *progressBar, QObject *parent)
    : Extractor(statusBar, progressBar, parent)
{
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

    emit statusMessage(tr("Packing atlas for Godot..."));
    emit progress(20);

    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    QString baseName = fileInfo.completeBaseName();
    QString imageFilename = baseName + ".png";
    QString imagePath = dir.filePath(imageFilename);

    AtlasPackResult packResult = AtlasPacker::pack(doc.frames(), 2);
    if (!packResult.success) {
        if (errorMsg) *errorMsg = QStringLiteral("Failed to pack atlas frames for Godot export.");
        return false;
    }

    emit progress(60);

    if (!packResult.atlas.save(imagePath, "PNG")) {
        if (errorMsg) *errorMsg = QString("Failed to write Godot atlas image: %1").arg(imagePath);
        return false;
    }

    emit progress(80);

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg) *errorMsg = QString("Cannot write to Godot resource file: %1").arg(filePath);
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

    bool firstAnim = true;
    for (auto it = doc.animations().begin(); it != doc.animations().end(); ++it) {
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

    emit progress(100);
    emit statusMessage(tr("Exported Godot resource: %1").arg(fileInfo.fileName()));
    return true;
}
