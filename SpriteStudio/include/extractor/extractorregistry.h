#ifndef EXTRACTORREGISTRY_H
#define EXTRACTORREGISTRY_H

#include <QObject>
#include <QList>
#include <QString>
#include "extractor/extractor.h"

/**
 * @brief Central registry managing built-in and dynamic Extractor plugins.
 */
class ExtractorRegistry : public QObject
{
    Q_OBJECT

public:
    static ExtractorRegistry& instance();
    ~ExtractorRegistry() override;

    void registerExtractor(Extractor *extractor, bool takeOwnership = true);
    void loadPlugins(const QString &dirPath);

    const QList<Extractor*>& extractors() const { return m_extractors; }

    Extractor* findDecoder(const QString &filePath) const;
    Extractor* findEncoder(const QString &filePathOrExt) const;
    Extractor* findEncoderByFilter(const QString &filter) const;
    Extractor* findExtractorById(const QString &id) const;

    QString openFilterString() const;
    QString saveFilterString() const;

    void initDefaultExtractors();

private:
    ExtractorRegistry() = default;
    Q_DISABLE_COPY(ExtractorRegistry)

    QList<Extractor*> m_extractors;
    QList<Extractor*> m_ownedExtractors;
    bool              m_initialized = false;
};

#endif // EXTRACTORREGISTRY_H
