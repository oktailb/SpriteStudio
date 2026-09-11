#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QImage>
#include <QDebug>

#include "model/spritedocument.h"
#include "extractor/extractor.h"
#include "extractor/extractorregistry.h"
#include "extractor/spriteextractor.h"
#include "extractor/gifextractor.h"
#include "extractor/jsonextractor.h"
#include "extractor/godotextractor.h"

class TestExtractors : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testExtractorRegistryBasics();
    void testSpriteExtractorCapabilities();
    void testSpriteExtractorReadPng();
    void testJsonExtractorReadWrite();
    void testGodotExtractorReadWrite();
    void testGifExtractorRead();
    void testErrorHandlingNonExistentFile();
    void testErrorHandlingCorruptedData();

private:
    QString m_sampleDir;
};

void TestExtractors::initTestCase()
{
    // Locate sample directory relative to current source or executable
    QStringList candidates = {
        QStringLiteral(SAMPLE_DIR),
        QDir::current().filePath(QStringLiteral("../sample")),
        QDir::current().filePath(QStringLiteral("../../sample")),
        QDir::current().filePath(QStringLiteral("sample"))
    };

    for (const QString &cand : candidates) {
        if (QFile::exists(cand + QStringLiteral("/ryu.png"))) {
            m_sampleDir = QDir(cand).canonicalPath();
            break;
        }
    }

    QVERIFY2(!m_sampleDir.isEmpty(), "Sample directory with test files could not be located");
    qDebug() << "Using sample directory:" << m_sampleDir;
}

void TestExtractors::testExtractorRegistryBasics()
{
    auto &reg = ExtractorRegistry::instance();
    const auto &extractors = reg.extractors();
    QVERIFY(extractors.size() >= 4);

    // Verify each codec is present
    QVERIFY(reg.findExtractorById(QStringLiteral("sprite_extractor")) != nullptr);
    QVERIFY(reg.findExtractorById(QStringLiteral("gif_extractor")) != nullptr);
    QVERIFY(reg.findExtractorById(QStringLiteral("json_extractor")) != nullptr);
    QVERIFY(reg.findExtractorById(QStringLiteral("godot_extractor")) != nullptr);

    // Verify filter strings
    QString openFilters = reg.openFilterString();
    QVERIFY(openFilters.contains(QStringLiteral("*.png")));
    QVERIFY(openFilters.contains(QStringLiteral("*.json")));
    QVERIFY(openFilters.contains(QStringLiteral("*.tres")));
    QVERIFY(openFilters.contains(QStringLiteral("*.gif")));

    QString saveFilters = reg.saveFilterString();
    QVERIFY(saveFilters.contains(QStringLiteral("*.json")));
    QVERIFY(saveFilters.contains(QStringLiteral("*.tres")));
}

void TestExtractors::testSpriteExtractorCapabilities()
{
    SpriteExtractor extractor;
    QCOMPARE(extractor.id(), QStringLiteral("sprite_extractor"));
    QCOMPARE(extractor.displayName(), QStringLiteral("Sprite Sheet"));
    QVERIFY(!extractor.version().isNull());
    QVERIFY(extractor.capabilities().testFlag(Extractor::CanImport));
    QVERIFY(extractor.capabilities().testFlag(Extractor::CanExport));
    QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("png")));
    QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("bmp")));
}

void TestExtractors::testSpriteExtractorReadPng()
{
    QString ryuPng = m_sampleDir + QStringLiteral("/ryu.png");
    QVERIFY(QFile::exists(ryuPng));

    SpriteExtractor extractor;
    SpriteDocument doc;
    ExtractorError err;

    QSignalSpy progressSpy(&extractor, &Extractor::progress);
    QSignalSpy statusSpy(&extractor, &Extractor::statusMessage);

    bool ok = extractor.read(ryuPng, doc, &err);
    QVERIFY2(ok, qPrintable(err.toString()));
    QVERIFY(!err.isError());

    // Document must contain loaded atlas and segmented frames
    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.frameCount() > 0);
    QCOMPARE(doc.boxes().size(), doc.frameCount());
    QVERIFY(doc.maxFrameWidth() > 0);
    QVERIFY(doc.maxFrameHeight() > 0);

    // Verify signals fired
    QVERIFY(!progressSpy.isEmpty());
    QVERIFY(!statusSpy.isEmpty());
}

void TestExtractors::testJsonExtractorReadWrite()
{
    QString ryuJson = m_sampleDir + QStringLiteral("/ryu.json");
    QVERIFY(QFile::exists(ryuJson));

    JsonExtractor extractor;
    SpriteDocument doc;
    ExtractorError err;

    bool ok = extractor.read(ryuJson, doc, &err);
    QVERIFY2(ok, qPrintable(err.toString()));
    QVERIFY(!err.isError());

    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.frameCount() > 0);
    QVERIFY(!doc.animations().isEmpty());

    // Test export round-trip to a temporary directory
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString exportedJson = tempDir.filePath(QStringLiteral("exported_ryu.json"));
    ExportOptions opts;
    opts.compressJson = false;

    ExtractorError writeErr;
    bool writeOk = extractor.write(exportedJson, doc, opts, &writeErr);
    QVERIFY2(writeOk, qPrintable(writeErr.toString()));
    QVERIFY(QFile::exists(exportedJson));
    QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("exported_ryu.png"))));

    // Read back exported JSON
    SpriteDocument doc2;
    ExtractorError readBackErr;
    bool readBackOk = extractor.read(exportedJson, doc2, &readBackErr);
    QVERIFY2(readBackOk, qPrintable(readBackErr.toString()));
    QCOMPARE(doc2.frameCount(), doc.frameCount());
    QCOMPARE(doc2.animations().size(), doc.animations().size());
}

void TestExtractors::testGodotExtractorReadWrite()
{
    QString godotTres = m_sampleDir + QStringLiteral("/ryu_godot.tres");
    QVERIFY(QFile::exists(godotTres));

    GodotExtractor extractor;
    SpriteDocument doc;
    ExtractorError err;

    bool ok = extractor.read(godotTres, doc, &err);
    QVERIFY2(ok, qPrintable(err.toString()));
    QVERIFY(!err.isError());

    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.frameCount() > 0);
    QVERIFY(!doc.animations().isEmpty());

    // Test export round-trip
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString exportedTres = tempDir.filePath(QStringLiteral("exported.tres"));
    ExportOptions opts;
    ExtractorError writeErr;

    bool writeOk = extractor.write(exportedTres, doc, opts, &writeErr);
    QVERIFY2(writeOk, qPrintable(writeErr.toString()));
    QVERIFY(QFile::exists(exportedTres));
    QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("exported.png"))));

    // Read back exported Godot resource
    SpriteDocument doc2;
    ExtractorError readBackErr;
    bool readBackOk = extractor.read(exportedTres, doc2, &readBackErr);
    QVERIFY2(readBackOk, qPrintable(readBackErr.toString()));
    QCOMPARE(doc2.frameCount(), doc.frameCount());
}

void TestExtractors::testGifExtractorRead()
{
    QString ryuGif = m_sampleDir + QStringLiteral("/ryu_hd.gif");
    QVERIFY(QFile::exists(ryuGif));

    GifExtractor extractor;
    SpriteDocument doc;
    ExtractorError err;

    bool ok = extractor.read(ryuGif, doc, &err);
    QVERIFY2(ok, qPrintable(err.toString()));
    QVERIFY(!err.isError());

    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.frameCount() > 1);
    QVERIFY(doc.hasAnimation(QStringLiteral("default")));
}

void TestExtractors::testErrorHandlingNonExistentFile()
{
    SpriteExtractor spriteExt;
    SpriteDocument doc;
    ExtractorError err;

    bool ok = spriteExt.read(QStringLiteral("non_existent_file_12345.png"), doc, &err);
    QVERIFY(!ok);
    QVERIFY(err.isError());
    QCOMPARE(err.code, ExtractorError::FileNotFound);
    QVERIFY(!err.message.isEmpty());

    JsonExtractor jsonExt;
    ok = jsonExt.read(QStringLiteral("non_existent_file_12345.json"), doc, &err);
    QVERIFY(!ok);
    QVERIFY(err.isError());
    QCOMPARE(err.code, ExtractorError::FileNotFound);

    GodotExtractor godotExt;
    ok = godotExt.read(QStringLiteral("non_existent_file_12345.tres"), doc, &err);
    QVERIFY(!ok);
    QVERIFY(err.isError());
    QCOMPARE(err.code, ExtractorError::FileNotFound);
}

void TestExtractors::testErrorHandlingCorruptedData()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString corruptedJson = tempDir.filePath(QStringLiteral("corrupted.json"));
    QFile f(corruptedJson);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("{ invalid json content ::: 123");
    f.close();

    JsonExtractor jsonExt;
    SpriteDocument doc;
    ExtractorError err;

    bool ok = jsonExt.read(corruptedJson, doc, &err);
    QVERIFY(!ok);
    QVERIFY(err.isError());
    QCOMPARE(err.code, ExtractorError::ParsingFailed);
}

#include <QGuiApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TestExtractors tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_extractors.moc"
