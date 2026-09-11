#include "extractor/extractor.h"
#include <QFileInfo>

Extractor::Extractor(QObject *parent)
    : QObject(parent)
{
}

void Extractor::setProgress(int percentage)
{
    m_progress = percentage;
    emit progress(percentage);
}

void Extractor::setStatusMessage(const QString &message)
{
    m_statusMessage = message;
    emit statusMessage(message);
}

bool Extractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    return supportedExtensions().contains(fi.suffix().toLower());
}

bool Extractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(inDoc);
    Q_UNUSED(options);
    if (error) {
        error->code = ExtractorError::UnsupportedFormat;
        error->message = tr("Export is not supported by this format (%1)").arg(displayName());
        error->filePath = filePath;
    }
    return false;
}

bool Extractor::extract(const QString &filePath, SpriteDocument &doc, QString *errorMsg)
{
    ExtractorError err;
    bool ok = read(filePath, doc, &err);
    if (!ok && errorMsg) {
        *errorMsg = err.toString();
    }
    return ok;
}

bool Extractor::exportDocument(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, QString *errorMsg)
{
    ExtractorError err;
    bool ok = write(filePath, doc, options, &err);
    if (!ok && errorMsg) {
        *errorMsg = err.toString();
    }
    return ok;
}
