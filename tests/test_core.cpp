#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QUndoStack>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

#include "model/spritedocument.h"
#include "packer/atlaspacker.h"
#include "commands/commands.h"
#include "config/appconfig.h"

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // AtlasPacker tests (7 tests)
    void testAtlasPackerEmpty();
    void testAtlasPackerSingleFrame();
    void testAtlasPackerRowPacker();
    void testAtlasPackerGridPacker();
    void testAtlasPackerPowerOfTwoPacker();
    void testAtlasPackerPackIndices();
    void testAtlasPackerPadding();

    // SpriteDocument tests (8 tests)
    void testDocumentClearAndEmpty();
    void testDocumentInsertAndReplaceFrame();
    void testDocumentRemoveFramesMulti();
    void testDocumentReorderFrames();
    void testDocumentMergeFrames();
    void testDocumentComputeTrimmedRect();
    void testDocumentProjectNameMultiplatform();
    void testDocumentAnimationsCRUD();

    // Commands tests (8 tests)
    void testCommandAddSlice();
    void testCommandChangeBoxRect();
    void testCommandCreateAnimation();
    void testCommandReverseAnimation();
    void testCommandDeleteAnimation();
    void testCommandMergeFrames();
    void testCommandDeleteFrames();
    void testCommandEraseAtlasPixels();

    // Multiplatform & System tests (3 tests)
    void testMultiplatformPathSeparators();
    void testMultiplatformImageFormats();
    void testMultiplatformAppConfigLocations();
};

void TestCore::initTestCase()
{
}

void TestCore::cleanupTestCase()
{
}

// =============================================================================
// AtlasPacker Tests
// =============================================================================

void TestCore::testAtlasPackerEmpty()
{
    QList<QPixmap> emptyFrames;
    AtlasPackResult res = AtlasPacker::pack(emptyFrames);
    QVERIFY(!res.success);
    QVERIFY(res.atlas.isNull());
    QVERIFY(res.frameRects.isEmpty());
}

void TestCore::testAtlasPackerSingleFrame()
{
    QImage img(32, 24, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::red);
    QPixmap pm = QPixmap::fromImage(img);

    AtlasPackResult res = AtlasPacker::pack({pm}, 4, AtlasPacker::RowPacker);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), 1);
    QCOMPARE(res.frameRects[0].width(), 32);
    QCOMPARE(res.frameRects[0].height(), 24);
    QVERIFY(res.atlas.width() >= 32 + 8);
    QVERIFY(res.atlas.height() >= 24 + 8);
}

void TestCore::testAtlasPackerRowPacker()
{
    QList<QPixmap> frames;
    QList<QSize> sizes = { QSize(16, 16), QSize(32, 48), QSize(64, 20), QSize(20, 60) };
    QList<QColor> colors = { Qt::red, Qt::green, Qt::blue, Qt::yellow };

    for (int i = 0; i < sizes.size(); ++i) {
        QImage img(sizes[i], QImage::Format_ARGB32_Premultiplied);
        img.fill(colors[i]);
        frames.append(QPixmap::fromImage(img));
    }

    AtlasPackResult res = AtlasPacker::pack(frames, 2, AtlasPacker::RowPacker);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), frames.size());

    // 1. All rects fit strictly inside atlas boundaries
    for (int i = 0; i < res.frameRects.size(); ++i) {
        QVERIFY(res.atlas.rect().contains(res.frameRects[i]));
        QCOMPARE(res.frameRects[i].size(), sizes[i]);
    }

    // 2. No frame rects overlap with each other
    for (int i = 0; i < res.frameRects.size(); ++i) {
        for (int j = i + 1; j < res.frameRects.size(); ++j) {
            QVERIFY(!res.frameRects[i].intersects(res.frameRects[j]));
        }
    }

    // 3. Pixel fidelity: sampled colors match
    for (int i = 0; i < res.frameRects.size(); ++i) {
        QPoint samplePoint = res.frameRects[i].center();
        QColor atlasColor = res.atlas.pixelColor(samplePoint);
        QCOMPARE(atlasColor.name(), colors[i].name());
    }
}

void TestCore::testAtlasPackerGridPacker()
{
    QList<QPixmap> frames;
    for (int i = 0; i < 6; ++i) {
        QImage img(24, 24, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::cyan);
        frames.append(QPixmap::fromImage(img));
    }

    AtlasPackResult res = AtlasPacker::pack(frames, 2, AtlasPacker::GridPacker);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), 6);

    for (int i = 0; i < res.frameRects.size(); ++i) {
        QVERIFY(res.atlas.rect().contains(res.frameRects[i]));
        for (int j = i + 1; j < res.frameRects.size(); ++j) {
            QVERIFY(!res.frameRects[i].intersects(res.frameRects[j]));
        }
    }
}

void TestCore::testAtlasPackerPowerOfTwoPacker()
{
    QList<QPixmap> frames;
    for (int i = 0; i < 5; ++i) {
        QImage img(35, 45, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::magenta);
        frames.append(QPixmap::fromImage(img));
    }

    AtlasPackResult res = AtlasPacker::pack(frames, 2, AtlasPacker::PowerOfTwoPacker);
    QVERIFY(res.success);

    // Verify width and height are strictly powers of two (crucial for GPUs across platforms)
    int w = res.atlas.width();
    int h = res.atlas.height();
    QVERIFY(w > 0 && (w & (w - 1)) == 0);
    QVERIFY(h > 0 && (h & (h - 1)) == 0);
}

void TestCore::testAtlasPackerPackIndices()
{
    QList<QPixmap> frames;
    for (int i = 0; i < 4; ++i) {
        QImage img(10 * (i + 1), 10 * (i + 1), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        frames.append(QPixmap::fromImage(img));
    }

    // Pack only frame indices 1 and 3 (sizes 20x20 and 40x40)
    AtlasPackResult res = AtlasPacker::packIndices(frames, {1, 3}, 2);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), 2);
    QCOMPARE(res.frameRects[0].size(), QSize(20, 20));
    QCOMPARE(res.frameRects[1].size(), QSize(40, 40));

    // Empty indices
    AtlasPackResult emptyRes = AtlasPacker::packIndices(frames, {}, 2);
    QVERIFY(!emptyRes.success);
}

void TestCore::testAtlasPackerPadding()
{
    QImage img(20, 20, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QPixmap pm = QPixmap::fromImage(img);

    int padding = 8;
    AtlasPackResult res = AtlasPacker::pack({pm, pm}, padding, AtlasPacker::RowPacker);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), 2);

    // Margins from atlas borders must be at least padding
    QVERIFY(res.frameRects[0].left() >= padding);
    QVERIFY(res.frameRects[0].top() >= padding);

    // Spacing between adjacent frames must be at least padding
    if (res.frameRects[0].top() == res.frameRects[1].top()) {
        int gap = res.frameRects[1].left() - res.frameRects[0].right();
        QVERIFY(gap >= padding);
    }
}

// =============================================================================
// SpriteDocument Tests
// =============================================================================

void TestCore::testDocumentClearAndEmpty()
{
    SpriteDocument doc;
    QVERIFY(doc.isEmpty());

    QImage atlas(100, 100, QImage::Format_ARGB32);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(0, 0, 50, 50));
    doc.setAnimation(QStringLiteral("idle"), {0}, 12, true);
    doc.setFilePath(QStringLiteral("/path/to/project.png"));

    QVERIFY(!doc.isEmpty());
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.animationNames().size(), 1);

    QSignalSpy spyReset(&doc, &SpriteDocument::documentReset);
    doc.clear();

    QVERIFY(doc.isEmpty());
    QCOMPARE(doc.frameCount(), 0);
    QCOMPARE(doc.boxes().size(), 0);
    QCOMPARE(doc.animationNames().size(), 0);
    QVERIFY(doc.filePath().isEmpty());
    QCOMPARE(spyReset.count(), 1);
}

void TestCore::testDocumentInsertAndReplaceFrame()
{
    SpriteDocument doc;
    QImage atlas(200, 200, QImage::Format_ARGB32);
    doc.setAtlas(atlas);

    QPixmap pm1(20, 20);
    QPixmap pm2(30, 40);
    QPixmap pmInsert(60, 25);
    QPixmap pmReplace(80, 15);

    doc.addFrame(pm1);
    doc.addFrame(pm2);
    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(doc.maxFrameWidth(), 30);
    QCOMPARE(doc.maxFrameHeight(), 40);

    // Insert at index 1
    SpriteBox insertBox;
    insertBox.rect = QRect(0, 0, 60, 25);
    doc.insertFrame(1, pmInsert, insertBox);
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(doc.frame(1).width(), 60);
    QCOMPARE(doc.maxFrameWidth(), 60);

    // Replace at index 0
    SpriteBox replaceBox;
    replaceBox.rect = QRect(0, 0, 80, 15);
    doc.replaceFrame(0, pmReplace, replaceBox);
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(doc.frame(0).width(), 80);
    QCOMPARE(doc.maxFrameWidth(), 80);
}

void TestCore::testDocumentRemoveFramesMulti()
{
    SpriteDocument doc;
    for (int i = 0; i < 4; ++i) {
        QPixmap pm(10 * (i + 1), 10);
        SpriteBox box;
        box.rect = QRect(i * 10, 0, 10 * (i + 1), 10);
        box.index = i;
        doc.addFrame(pm, box);
    }
    QCOMPARE(doc.frameCount(), 4);

    // Remove frames at discontinuous indices {1, 3}
    doc.removeFrames({1, 3});
    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(doc.frame(0).width(), 10);
    QCOMPARE(doc.frame(1).width(), 30);
}

void TestCore::testDocumentReorderFrames()
{
    SpriteDocument doc;
    for (int i = 0; i < 3; ++i) {
        QPixmap pm(10 * (i + 1), 10);
        SpriteBox box;
        box.rect = QRect(i * 10, 0, 10 * (i + 1), 10);
        box.index = i;
        doc.addFrame(pm, box);
    }

    doc.setAnimation(QStringLiteral("run"), {0, 1, 2}, 12, true);

    // New order: index 2 becomes 0, index 0 becomes 1, index 1 becomes 2
    doc.reorderFrames({2, 0, 1});
    QCOMPARE(doc.frame(0).width(), 30);
    QCOMPARE(doc.frame(1).width(), 10);
    QCOMPARE(doc.frame(2).width(), 20);

    // Animation frame indices mapped automatically
    QCOMPARE(doc.animation(QStringLiteral("run")).frameIndices, (QList<int>{1, 2, 0}));
}

void TestCore::testDocumentMergeFrames()
{
    SpriteDocument doc;
    QImage atlas(200, 200, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    doc.addSlice(QRect(10, 10, 20, 20)); // index 0
    doc.addSlice(QRect(40, 10, 20, 20)); // index 1

    QCOMPARE(doc.frameCount(), 2);

    // Merge index 0 into index 1
    doc.mergeFrames(0, 1);
    QCOMPARE(doc.frameCount(), 1);

    // The merged box should unite QRect(10, 10, 20, 20) and QRect(40, 10, 20, 20) => QRect(10, 10, 50, 20)
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 50, 20));
}

void TestCore::testDocumentComputeTrimmedRect()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);

    // Draw opaque content at (30, 20, 40, 50)
    for (int y = 20; y < 70; ++y) {
        for (int x = 30; x < 70; ++x) {
            atlas.setPixelColor(x, y, QColor(255, 0, 0, 255));
        }
    }
    doc.setAtlas(atlas);

    // Add a larger slice that encompasses the content with transparent padding
    doc.addSlice(QRect(10, 5, 80, 90));

    // Trim with alphaThreshold = 1
    QRect trimmed = doc.computeTrimmedRect(0, 1);
    QCOMPARE(trimmed, QRect(30, 20, 40, 50));

    // Fully transparent slice returns original rect
    doc.addSlice(QRect(0, 0, 15, 15));
    QRect trimmedEmpty = doc.computeTrimmedRect(1, 1);
    QCOMPARE(trimmedEmpty, QRect(0, 0, 15, 15));
}

void TestCore::testDocumentProjectNameMultiplatform()
{
    SpriteDocument doc;
    QCOMPARE(doc.projectName(), QStringLiteral("untitled"));

    // 1. Windows path syntax
    doc.setFilePath(QStringLiteral("C:\\Users\\Artist\\Sprites\\hero_idle.png"));
    QCOMPARE(doc.projectName(), QStringLiteral("hero_idle"));

    // 2. Linux / Unix path syntax
    doc.setFilePath(QStringLiteral("/home/developer/games/assets/monster_walk.tres"));
    QCOMPARE(doc.projectName(), QStringLiteral("monster_walk"));

    // 3. Apple macOS path syntax
    doc.setFilePath(QStringLiteral("/Users/designer/Desktop/boss_attack.gif"));
    QCOMPARE(doc.projectName(), QStringLiteral("boss_attack"));

    // 4. Haiku path syntax
    doc.setFilePath(QStringLiteral("/boot/home/config/settings/particles.json"));
    QCOMPARE(doc.projectName(), QStringLiteral("particles"));

    // 5. Multi-dot extension handling
    doc.setFilePath(QStringLiteral("archive.sheet.v1.0.png"));
    QCOMPARE(doc.projectName(), QStringLiteral("archive.sheet.v1.0"));
}

void TestCore::testDocumentAnimationsCRUD()
{
    SpriteDocument doc;
    QVERIFY(!doc.hasAnimation(QStringLiteral("walk")));

    QSignalSpy spyAnim(&doc, &SpriteDocument::animationsChanged);

    doc.setAnimation(QStringLiteral("walk"), {0, 1, 2}, 16, true);
    QVERIFY(doc.hasAnimation(QStringLiteral("walk")));
    QCOMPARE(doc.animation(QStringLiteral("walk")).fps, 16);
    QVERIFY(doc.animation(QStringLiteral("walk")).loop);
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 2}));
    QVERIFY(spyAnim.count() >= 1);

    // Reversing animation frames
    doc.reverseAnimationFrames(QStringLiteral("walk"));
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{2, 1, 0}));

    // Removing animation
    doc.removeAnimation(QStringLiteral("walk"));
    QVERIFY(!doc.hasAnimation(QStringLiteral("walk")));
}

// =============================================================================
// Commands Undo/Redo Tests
// =============================================================================

void TestCore::testCommandAddSlice()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    QUndoStack undoStack;
    undoStack.push(new AddSliceCommand(&doc, QRect(10, 10, 30, 30)));
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 30, 30));

    undoStack.undo();
    QCOMPARE(doc.frameCount(), 0);

    undoStack.redo();
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 30, 30));
}

void TestCore::testCommandChangeBoxRect()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20));

    QUndoStack undoStack;
    undoStack.push(new ChangeBoxRectCommand(&doc, 0, QRect(10, 10, 20, 20), QRect(15, 25, 40, 50)));
    QCOMPARE(doc.box(0).rect, QRect(15, 25, 40, 50));

    undoStack.undo();
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 20, 20));

    undoStack.redo();
    QCOMPARE(doc.box(0).rect, QRect(15, 25, 40, 50));
}

void TestCore::testCommandCreateAnimation()
{
    SpriteDocument doc;
    QUndoStack undoStack;

    undoStack.push(new CreateAnimationCommand(&doc, QStringLiteral("attack"), {0, 1, 2}, 24));
    QVERIFY(doc.hasAnimation(QStringLiteral("attack")));
    QCOMPARE(doc.animation(QStringLiteral("attack")).fps, 24);

    undoStack.undo();
    QVERIFY(!doc.hasAnimation(QStringLiteral("attack")));

    undoStack.redo();
    QVERIFY(doc.hasAnimation(QStringLiteral("attack")));
}

void TestCore::testCommandReverseAnimation()
{
    SpriteDocument doc;
    doc.setAnimation(QStringLiteral("idle"), {0, 1, 2, 3}, 12, true);

    QUndoStack undoStack;
    undoStack.push(new ReverseAnimationCommand(&doc, QStringLiteral("idle")));
    QCOMPARE(doc.animation(QStringLiteral("idle")).frameIndices, (QList<int>{3, 2, 1, 0}));

    undoStack.undo();
    QCOMPARE(doc.animation(QStringLiteral("idle")).frameIndices, (QList<int>{0, 1, 2, 3}));

    undoStack.redo();
    QCOMPARE(doc.animation(QStringLiteral("idle")).frameIndices, (QList<int>{3, 2, 1, 0}));
}

void TestCore::testCommandDeleteAnimation()
{
    SpriteDocument doc;
    doc.setAnimation(QStringLiteral("die"), {4, 5}, 8, false);

    QUndoStack undoStack;
    undoStack.push(new DeleteAnimationCommand(&doc, QStringLiteral("die")));
    QVERIFY(!doc.hasAnimation(QStringLiteral("die")));

    undoStack.undo();
    QVERIFY(doc.hasAnimation(QStringLiteral("die")));
    QCOMPARE(doc.animation(QStringLiteral("die")).fps, 8);
    QVERIFY(!doc.animation(QStringLiteral("die")).loop);

    undoStack.redo();
    QVERIFY(!doc.hasAnimation(QStringLiteral("die")));
}

void TestCore::testCommandMergeFrames()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(5, 5, 20, 20));  // index 0
    doc.addSlice(QRect(30, 5, 20, 20)); // index 1

    QUndoStack undoStack;
    undoStack.push(new MergeFramesCommand(&doc, 0, 1));
    QCOMPARE(doc.frameCount(), 1);

    undoStack.undo();
    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(doc.box(0).rect, QRect(5, 5, 20, 20));
    QCOMPARE(doc.box(1).rect, QRect(30, 5, 20, 20));

    undoStack.redo();
    QCOMPARE(doc.frameCount(), 1);
}

void TestCore::testCommandDeleteFrames()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(0, 0, 10, 10));
    doc.addSlice(QRect(20, 0, 10, 10));
    doc.addSlice(QRect(40, 0, 10, 10));

    QUndoStack undoStack;
    undoStack.push(new DeleteFramesCommand(&doc, {0, 2}));
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.box(0).rect, QRect(20, 0, 10, 10));

    undoStack.undo();
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(doc.box(0).rect, QRect(0, 0, 10, 10));
    QCOMPARE(doc.box(1).rect, QRect(20, 0, 10, 10));
    QCOMPARE(doc.box(2).rect, QRect(40, 0, 10, 10));

    undoStack.redo();
    QCOMPARE(doc.frameCount(), 1);
}

void TestCore::testCommandEraseAtlasPixels()
{
    SpriteDocument doc;
    QImage atlas(50, 50, QImage::Format_ARGB32);
    atlas.fill(QColor(255, 128, 0, 255));
    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20));

    QUndoStack undoStack;
    undoStack.push(new EraseAtlasPixelsCommand(&doc, {0}));
    QCOMPARE(doc.frameCount(), 0);
    // Erased region is transparent
    QCOMPARE(qAlpha(doc.atlas().pixel(15, 15)), 0);
    // Non-erased region is opaque
    QCOMPARE(qAlpha(doc.atlas().pixel(2, 2)), 255);

    undoStack.undo();
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(qAlpha(doc.atlas().pixel(15, 15)), 255);

    undoStack.redo();
    QCOMPARE(doc.frameCount(), 0);
    QCOMPARE(qAlpha(doc.atlas().pixel(15, 15)), 0);
}

// =============================================================================
// Multiplatform & System Tests
// =============================================================================

void TestCore::testMultiplatformPathSeparators()
{
    // Ensure mixing forward slashes and backslashes is normalized properly across OS
    QString mixedPath = QStringLiteral("assets/sprites\\level1/hero.png");
    QString unifiedPath = QDir::fromNativeSeparators(mixedPath);
    QVERIFY(!unifiedPath.contains(QLatin1Char('\\')));
    QCOMPARE(QFileInfo(unifiedPath).fileName(), QStringLiteral("hero.png"));

    // Case insensitivity extension check simulation
    QStringList validExts = { QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("gif"), QStringLiteral("json") };
    QString fileUpper = QStringLiteral("MY_SPRITE.PNG");
    QString ext = QFileInfo(fileUpper).suffix().toLower();
    QVERIFY(validExts.contains(ext));
}

void TestCore::testMultiplatformImageFormats()
{
    // Validate 32-bit ARGB memory alignment and alpha fidelity across architectures
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(qRgba(120, 200, 50, 180));

    // Verify alpha is preserved exactly
    QCOMPARE(qAlpha(img.pixel(2, 2)), 180);
    QCOMPARE(qRed(img.pixel(2, 2)), 120);
    QCOMPARE(qGreen(img.pixel(2, 2)), 200);
    QCOMPARE(qBlue(img.pixel(2, 2)), 50);

    // Scanline pointer memory step is 4 bytes per pixel
    const uchar *scan0 = img.constScanLine(0);
    const uchar *scan1 = img.constScanLine(1);
    QCOMPARE(scan1 - scan0, 4 * sizeof(QRgb));
}

void TestCore::testMultiplatformAppConfigLocations()
{
    AppConfig &cfg = AppConfig::instance();
    QString path = cfg.configFilePath();
    QVERIFY(!path.isEmpty());

    // Saving and loading in temporary cross-platform directory
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString testCfgPath = tempDir.filePath(QStringLiteral("config_portable.json"));

    cfg.setConfigFilePath(testCfgPath);
    cfg.atlas().zoomStep = 1.35;
    QVERIFY(cfg.save());
    QVERIFY(QFile::exists(testCfgPath));

    cfg.resetToDefaults();
    QCOMPARE(cfg.atlas().zoomStep, 1.15);

    QVERIFY(cfg.load());
    QCOMPARE(cfg.atlas().zoomStep, 1.35);

    // Cleanup config path
    cfg.setConfigFilePath(QString());
}

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestCore tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_core.moc"
