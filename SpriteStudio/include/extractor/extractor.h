/**
 * @file extractor.h
 * @brief Defines the Extractor base class, the pure I/O codec plugin interface for SpriteStudio.
 */

#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVersionNumber>
#include <QImage>
#include <QPixmap>
#include <QList>
#include "export.h"
#include "model/spritedocument.h"

/**
 * @brief Structured error reporting for extractor operations.
 */
struct ExtractorError {
    enum Code {
        NoError = 0,
        FileNotFound,
        FileNotReadable,
        FileNotWritable,
        InvalidHeader,
        CorruptedData,
        UnsupportedVersion,
        UnsupportedFormat,
        ImageLoadFailed,
        ParsingFailed,
        PackingFailed,
        WriteFailed
    };

    Code    code = NoError;
    QString message;
    QString filePath;

    bool isError() const { return code != NoError; }
    QString toString() const {
        if (code == NoError) return QStringLiteral("Success");
        return filePath.isEmpty() ? message : QStringLiteral("%1: %2").arg(filePath, message);
    }
};

/**
 * @brief Parameters for static sprite sheet edge detection and segmentation.
 */
struct SpriteSheetOptions {
    enum CropStrategy {
        MergeStrategy,
        SeparateStrategy,
        BoundaryStrategy,
        AlphaChannelStrategy
    };

    int          alphaThreshold = 20;
    int          verticalTolerance = 5;
    bool         smartCrop = true;
    double       overlapThreshold = 0.1;
    CropStrategy cropStrategy = SeparateStrategy;
};

/**
 * @brief Pure I/O Codec Plugin Interface for all sprite formats.
 *
 * Each Extractor translates external file representations (PNG, GIF, JSON, Godot tres, etc.)
 * directly into or from a central SpriteDocument. It holds NO internal document state.
 */
class Extractor : public QObject
{
    Q_OBJECT

public:
    enum Capability {
        CanImport             = 0x01,
        CanExport             = 0x02,
        SupportsAnimations    = 0x04,
        SupportsAtlasMetadata = 0x08
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    // Alias for backward compatibility
    using Box = SpriteBox;

    explicit Extractor(QObject *parent = nullptr);
    ~Extractor() override = default;

    // Metadata & Introspection
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual QString description() const = 0;
    virtual QVersionNumber version() const { return QVersionNumber(1, 0, 0); }
    virtual QStringList supportedExtensions() const = 0;
    virtual Capabilities capabilities() const { return CanImport; }

    // Format detection
    virtual bool canDecode(const QString &filePath) const;

    // Primary I/O contract
    virtual bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) = 0;
    virtual bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr);

    // Compatibility wrappers
    bool extract(const QString &filePath, SpriteDocument &doc, QString *errorMsg = nullptr);
    bool exportDocument(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, QString *errorMsg = nullptr);

    // Progress & Status API
    int currentProgress() const { return m_progress; }
    QString currentStatusMessage() const { return m_statusMessage; }

protected:
    void setProgress(int percentage);
    void setStatusMessage(const QString &message);

private:
    int     m_progress = 0;
    QString m_statusMessage;

signals:
    void progress(int percentage);
    void statusMessage(const QString &message);
    void extractionFinished(int frameCount);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Extractor::Capabilities)

#define Extractor_iid "com.spritestudio.Extractor/2.0"
Q_DECLARE_INTERFACE(Extractor, Extractor_iid)

#endif // EXTRACTOR_H
