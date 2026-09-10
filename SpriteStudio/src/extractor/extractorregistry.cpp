#include "extractor/extractorregistry.h"
#include "extractor/spriteextractor.h"
#include "extractor/gifextractor.h"
#include "extractor/jsonextractor.h"
#include "extractor/godotextractor.h"
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QCoreApplication>

ExtractorRegistry& ExtractorRegistry::instance()
{
    static ExtractorRegistry reg;
    if (!reg.m_initialized) {
        reg.initDefaultExtractors();
    }
    return reg;
}

ExtractorRegistry::~ExtractorRegistry()
{
    for (Extractor *ext : m_ownedExtractors) {
        delete ext;
    }
    m_ownedExtractors.clear();
    m_extractors.clear();
}

void ExtractorRegistry::registerExtractor(Extractor *extractor, bool takeOwnership)
{
    if (!extractor || m_extractors.contains(extractor)) {
        return;
    }

    m_extractors.append(extractor);
    if (takeOwnership) {
        m_ownedExtractors.append(extractor);
    }
}

void ExtractorRegistry::initDefaultExtractors()
{
    m_initialized = true;

    // 1. Static sprite sheets (PNG, JPG, BMP)
    registerExtractor(new SpriteExtractor(), true);

    // 2. Animated GIF
    registerExtractor(new GifExtractor(), true);

    // 3. TexturePacker & Aseprite JSON
    registerExtractor(new JsonExtractor(), true);

    // 4. Godot Engine 4.x SpriteFrames (.tres)
    registerExtractor(new GodotExtractor(), true);

    // 5. Look for external dynamic plugins in plugins/ directory
    QString pluginsPath = QDir(QCoreApplication::applicationDirPath()).filePath("plugins");
    loadPlugins(pluginsPath);
}

void ExtractorRegistry::loadPlugins(const QString &dirPath)
{
    QDir pluginsDir(dirPath);
    if (!pluginsDir.exists()) return;

    for (const QString &fileName : pluginsDir.entryList(QDir::Files)) {
        QString fullPath = pluginsDir.absoluteFilePath(fileName);
        QPluginLoader loader(fullPath);
        QObject *plugin = loader.instance();
        if (plugin) {
            Extractor *ext = qobject_cast<Extractor*>(plugin);
            if (ext) {
                registerExtractor(ext, false);
            }
        }
    }
}

Extractor* ExtractorRegistry::findDecoder(const QString &filePath) const
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    for (Extractor *extractor : m_extractors) {
        if ((extractor->capabilities() & Extractor::CanImport) &&
            extractor->supportedExtensions().contains(ext) &&
            extractor->canDecode(filePath)) {
            return extractor;
        }
    }

    for (Extractor *extractor : m_extractors) {
        if ((extractor->capabilities() & Extractor::CanImport) && extractor->canDecode(filePath)) {
            return extractor;
        }
    }

    return nullptr;
}

Extractor* ExtractorRegistry::findEncoder(const QString &filePathOrExt) const
{
    QString ext = filePathOrExt.contains('.') ? QFileInfo(filePathOrExt).suffix().toLower() : filePathOrExt.toLower();

    for (Extractor *extractor : m_extractors) {
        if ((extractor->capabilities() & Extractor::CanExport) && extractor->supportedExtensions().contains(ext)) {
            return extractor;
        }
    }
    return nullptr;
}

Extractor* ExtractorRegistry::findEncoderByFilter(const QString &filter) const
{
    if (filter.isEmpty()) return nullptr;

    for (Extractor *extractor : m_extractors) {
        if ((extractor->capabilities() & Extractor::CanExport) &&
            filter.contains(extractor->displayName(), Qt::CaseInsensitive)) {
            return extractor;
        }
    }
    return nullptr;
}

Extractor* ExtractorRegistry::findExtractorById(const QString &id) const
{
    for (Extractor *extractor : m_extractors) {
        if (extractor->id() == id) {
            return extractor;
        }
    }
    return nullptr;
}

QString ExtractorRegistry::openFilterString() const
{
    QString allExtensions;
    QStringList individualFilters;

    for (Extractor *extractor : m_extractors) {
        if (!(extractor->capabilities() & Extractor::CanImport)) continue;

        QStringList wildcards;
        for (const QString &ext : extractor->supportedExtensions()) {
            wildcards.append("*." + ext);
            if (!allExtensions.contains("*." + ext)) {
                if (!allExtensions.isEmpty()) allExtensions += " ";
                allExtensions += "*." + ext;
            }
        }
        QString name = extractor->displayName();
        if (!name.contains('(')) {
            name = QString("%1 (%2)").arg(name, wildcards.join(" "));
        }
        individualFilters.append(name);
    }

    QString result;
    if (!allExtensions.isEmpty()) {
        result = QString("All Supported Files (%1);;").arg(allExtensions);
    }
    result += individualFilters.join(";;");
    return result;
}

QString ExtractorRegistry::saveFilterString() const
{
    QStringList individualFilters;

    for (Extractor *extractor : m_extractors) {
        if (!(extractor->capabilities() & Extractor::CanExport)) continue;

        QStringList wildcards;
        for (const QString &ext : extractor->supportedExtensions()) {
            wildcards.append("*." + ext);
        }
        QString name = extractor->displayName();
        if (!name.contains('(')) {
            name = QString("%1 (%2)").arg(name, wildcards.join(" "));
        }
        individualFilters.append(name);
    }

    return individualFilters.join(";;");
}
