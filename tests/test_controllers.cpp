#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QGraphicsView>
#include <QUndoStack>
#include <QContextMenuEvent>
#include <QTranslator>

#include "model/spritedocument.h"
#include "controller/projectcontroller.h"
#include "controller/animationcontroller.h"
#include "controller/atlasviewcontroller.h"
#include "animation/animationplayer.h"
#include "config/appconfig.h"
#include "commands/commands.h"
#include "atlasboxitem.h"

class TestControllers : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // AppConfig tests
    void testAppConfigDefaults();
    void testAppConfigSaveAndLoad();
    void testAppConfigCorruptJsonFallback();

    // ProjectController tests
    void testProjectControllerOpenJson();
    void testProjectControllerOpenGif();
    void testProjectControllerOpenNonExistent();
    void testProjectControllerRecentFiles();
    void testProjectControllerBackgroundRemoval();
    void testProjectControllerOpenAsync();
    void testProjectControllerRemoveBgAsync();
    void testUndoStackLimitAndImageStorage();

    // AnimationController tests
    void testAnimationControllerPlayback();
    void testAnimationControllerCreateAnimation();
    void testAnimationControllerReverseAnimation();
    void testAnimationControllerRemoveAnimation();
    void testAnimationControllerCurrentSelection();
    void testAnimationControllerAutoPlay();

    // AtlasViewController tests
    void testAtlasViewControllerToolMode();
    void testAtlasViewControllerZoom();
    void testAtlasViewControllerBoxSync();
    void testAtlasViewControllerSelection();
    void testAtlasViewControllerNudge();
    void testAtlasViewControllerTrimAndMerge();
    void testAtlasViewControllerErasePixels();
    void testAtlasViewControllerMarqueeSelection();
    void testAtlasViewControllerContextMenuSignals();
    void testControllerCrossSyncNoRecursion();
    void testAtlasViewControllerMultiSelectAndDelete();
    void testAtlasViewControllerMouseCenteredZoom();
    void testI18nKeyTranslations();
    void testAtlasBoxItemHandleCosmeticSize();
    void testAtlasViewControllerGroupDrag();
    void testAtlasViewControllerContinuousSlice();

private:
    QString m_sampleDir;
};

void TestControllers::initTestCase()
{
    m_sampleDir = QStringLiteral(SAMPLE_DIR);
    QVERIFY(!m_sampleDir.isEmpty());
}

void TestControllers::cleanupTestCase()
{
}

// -----------------------------------------------------------------------------
// AppConfig Tests
// -----------------------------------------------------------------------------

void TestControllers::testAppConfigDefaults()
{
    AppConfig &cfg = AppConfig::instance();
    cfg.resetToDefaults();

    // Atlas defaults
    QCOMPARE(cfg.atlas().zoomMin, 0.1);
    QCOMPARE(cfg.atlas().zoomMax, 10.0);
    QCOMPARE(cfg.atlas().zoomStep, 1.15);
    QCOMPARE(cfg.atlas().minSliceSize, 3);
    QCOMPARE(cfg.atlas().defaultAlphaThreshold, 1);
    QCOMPARE(cfg.atlas().defaultVerticalTolerance, 0);
    QCOMPARE(cfg.atlas().nudgeStepSmall, 1);
    QCOMPARE(cfg.atlas().nudgeStepLarge, 10);
    QCOMPARE(cfg.atlas().fitViewPadding, 20);

    // Visuals defaults
    QCOMPARE(cfg.visuals().handleSize, 8.0);
    QCOMPARE(cfg.visuals().handleMargin, 16.0);
    QCOMPARE(cfg.visuals().selectedBoxColor, QColor(255, 200, 0));

    // Animation defaults
    QCOMPARE(cfg.animation().defaultFps, 12);
    QCOMPARE(cfg.animation().minFps, 1);
    QCOMPARE(cfg.animation().maxFps, 60);

    // Project defaults
    QCOMPARE(cfg.project().maxRecentFiles, 10);
    QCOMPARE(cfg.project().backgroundRemovalTolerance, 10);
    QCOMPARE(cfg.project().backgroundMinAlpha, 10);
}

void TestControllers::testAppConfigSaveAndLoad()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString tempConfigPath = tempDir.filePath(QStringLiteral("test_config.json"));

    AppConfig &cfg = AppConfig::instance();
    cfg.resetToDefaults();

    // Modify some values
    cfg.atlas().zoomMax = 20.0;
    cfg.atlas().minSliceSize = 5;
    cfg.animation().defaultFps = 24;
    cfg.project().maxRecentFiles = 15;
    cfg.visuals().selectedBoxColor = QColor(255, 0, 0);

    // Save to temp path
    bool saveOk = cfg.save(tempConfigPath);
    QVERIFY(saveOk);
    QVERIFY(QFile::exists(tempConfigPath));

    // Reset to defaults
    cfg.resetToDefaults();
    QCOMPARE(cfg.atlas().zoomMax, 10.0);
    QCOMPARE(cfg.animation().defaultFps, 12);

    // Load back from temp path
    bool loadOk = cfg.load(tempConfigPath);
    QVERIFY(loadOk);

    // Verify modified values are recovered
    QCOMPARE(cfg.atlas().zoomMax, 20.0);
    QCOMPARE(cfg.atlas().minSliceSize, 5);
    QCOMPARE(cfg.animation().defaultFps, 24);
    QCOMPARE(cfg.project().maxRecentFiles, 15);
    QCOMPARE(cfg.visuals().selectedBoxColor, QColor(255, 0, 0));

    // Clean up
    cfg.resetToDefaults();
}

void TestControllers::testAppConfigCorruptJsonFallback()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString corruptPath = tempDir.filePath(QStringLiteral("corrupt.json"));

    // Write incomplete / invalid JSON
    {
        QFile file(corruptPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("{ \"atlas\": { \"zoom_max\": 999.0, INVALID SYNTAX ... ");
        file.close();
    }

    AppConfig &cfg = AppConfig::instance();
    cfg.resetToDefaults();

    // Loading corrupt JSON must fail gracefully without throwing or crashing
    bool loadOk = cfg.load(corruptPath);
    QVERIFY(!loadOk);

    // Safe defaults must be preserved
    QCOMPARE(cfg.atlas().zoomMax, 10.0);
    QCOMPARE(cfg.animation().defaultFps, 12);
}

// -----------------------------------------------------------------------------
// ProjectController Tests
// -----------------------------------------------------------------------------

void TestControllers::testProjectControllerOpenJson()
{
    QString jsonPath = m_sampleDir + QStringLiteral("/ryu.json");
    QVERIFY(QFile::exists(jsonPath));

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);

    QSignalSpy spyLoaded(&controller, &ProjectController::fileLoaded);
    QSignalSpy spyError(&controller, &ProjectController::fileLoadError);

    QString errorMsg;
    bool ok = controller.openFile(jsonPath, &errorMsg);
    QVERIFY2(ok, qPrintable(errorMsg));
    QCOMPARE(spyLoaded.count(), 1);
    QCOMPARE(spyError.count(), 0);
    QCOMPARE(controller.currentFilePath(), jsonPath);
    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.frameCount() > 0);
}

void TestControllers::testProjectControllerOpenGif()
{
    QString gifPath = m_sampleDir + QStringLiteral("/ryu_hd.gif");
    QVERIFY(QFile::exists(gifPath));

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);

    QSignalSpy spyLoaded(&controller, &ProjectController::fileLoaded);

    bool ok = controller.openFile(gifPath);
    QVERIFY(ok);
    QCOMPARE(spyLoaded.count(), 1);
    QVERIFY(doc.frameCount() > 1);
}

void TestControllers::testProjectControllerOpenNonExistent()
{
    SpriteDocument doc;
    ProjectController controller(&doc);

    QSignalSpy spyLoaded(&controller, &ProjectController::fileLoaded);
    QSignalSpy spyError(&controller, &ProjectController::fileLoadError);

    QString errorMsg;
    bool ok = controller.openFile(QStringLiteral("non_existent_file_98765.json"), &errorMsg);
    QVERIFY(!ok);
    QCOMPARE(spyLoaded.count(), 0);
    QCOMPARE(spyError.count(), 1);
    QVERIFY(!errorMsg.isEmpty());
}

void TestControllers::testProjectControllerRecentFiles()
{
    SpriteDocument doc;
    ProjectController controller(&doc);

    controller.clearRecentFiles();
    QVERIFY(controller.recentFiles().isEmpty());

    // Create 12 temporary dummy files
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QStringList createdFiles;
    for (int i = 1; i <= 12; ++i) {
        QString fpath = tempDir.filePath(QStringLiteral("file_%1.png").arg(i));
        QFile f(fpath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("dummy");
        f.close();
        createdFiles.append(fpath);
        controller.addRecentFile(fpath);
    }

    QStringList recent = controller.recentFiles();
    // Maximum 10 recent files
    QCOMPARE(recent.size(), 10);
    // Most recently added is at index 0
    QCOMPARE(recent.first(), createdFiles.last());

    controller.clearRecentFiles();
    QVERIFY(controller.recentFiles().isEmpty());
}

void TestControllers::testProjectControllerBackgroundRemoval()
{
    // Create an image: red background (255, 0, 0), with a blue 10x10 square
    QImage testImg(40, 40, QImage::Format_ARGB32);
    testImg.fill(qRgb(255, 0, 0));

    QPainter p(&testImg);
    p.fillRect(10, 10, 10, 10, QColor(0, 0, 255));
    p.end();

    QImage cleaned = ProjectController::removeBackgroundFromImage(testImg, 10);
    QVERIFY(!cleaned.isNull());

    // Corner pixel (background) should now be transparent
    QRgb cornerPixel = cleaned.pixel(0, 0);
    QCOMPARE(qAlpha(cornerPixel), 0);

    // Blue square pixel should still be fully opaque blue
    QRgb centerPixel = cleaned.pixel(15, 15);
    QCOMPARE(qAlpha(centerPixel), 255);
    QCOMPARE(qBlue(centerPixel), 255);

    // Test controller removeAtlasBackgroundAndRefresh with multi-sprite document
    SpriteDocument doc;
    QImage testImg2(60, 60, QImage::Format_ARGB32);
    testImg2.fill(qRgb(255, 0, 0));
    QPainter p2(&testImg2);
    p2.fillRect(5, 5, 10, 10, QColor(0, 0, 255));
    p2.fillRect(35, 35, 10, 10, QColor(0, 255, 0));
    p2.end();

    doc.setAtlas(testImg2);
    ProjectController controller(&doc);
    QSignalSpy spyBg(&controller, &ProjectController::backgroundRemoved);

    bool ok = controller.removeAtlasBackgroundAndRefresh(10, 5, false, 0.5);
    QVERIFY(ok);
    QCOMPARE(spyBg.count(), 1);
    QCOMPARE(doc.frameCount(), 2);
}

void TestControllers::testProjectControllerOpenAsync()
{
    QString pngPath = m_sampleDir + QStringLiteral("/ryu.png");
    QVERIFY(QFile::exists(pngPath));

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);

    QSignalSpy spyStarted(&controller, &ProjectController::processingStarted);
    QSignalSpy spyLoaded(&controller, &ProjectController::fileLoaded);
    QSignalSpy spyFinished(&controller, &ProjectController::processingFinished);
    QSignalSpy spyError(&controller, &ProjectController::fileLoadError);

    controller.openFileAsync(pngPath);

    // Wait for the async worker thread and main-thread finish
    QVERIFY(spyLoaded.wait(5000));
    QCOMPARE(spyStarted.count(), 1);
    QCOMPARE(spyLoaded.count(), 1);
    QCOMPARE(spyFinished.count(), 1);
    QCOMPARE(spyError.count(), 0);
    QCOMPARE(controller.currentFilePath(), pngPath);
    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.frameCount() > 0);
}

void TestControllers::testProjectControllerRemoveBgAsync()
{
    SpriteDocument doc;
    QImage testImg(60, 60, QImage::Format_ARGB32);
    testImg.fill(qRgb(255, 0, 0));
    QPainter p(&testImg);
    p.fillRect(5, 5, 10, 10, QColor(0, 0, 255));
    p.fillRect(35, 35, 10, 10, QColor(0, 255, 0));
    p.end();

    doc.setAtlas(testImg);
    ProjectController controller(&doc);

    QSignalSpy spyStarted(&controller, &ProjectController::processingStarted);
    QSignalSpy spyBg(&controller, &ProjectController::backgroundRemoved);
    QSignalSpy spyFinished(&controller, &ProjectController::processingFinished);

    controller.removeAtlasBackgroundAndRefreshAsync(10, 5, false, 0.5);

    QVERIFY(spyBg.wait(5000));
    QCOMPARE(spyStarted.count(), 1);
    QCOMPARE(spyBg.count(), 1);
    QCOMPARE(spyFinished.count(), 1);
    QCOMPARE(doc.frameCount(), 2);
    // Background pixel (0,0) must now be transparent
    QCOMPARE(qAlpha(doc.atlas().pixel(0, 0)), 0);
}

void TestControllers::testUndoStackLimitAndImageStorage()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    // Verify AppConfig undoLimit default is 50
    QCOMPARE(AppConfig::instance().project().undoLimit, 50);

    QUndoStack undoStack;
    undoStack.setUndoLimit(AppConfig::instance().project().undoLimit);
    QCOMPARE(undoStack.undoLimit(), 50);

    // Populate initial slices
    for (int i = 0; i < 65; ++i) {
        doc.addSlice(QRect(0, 0, 5, 5));
    }

    // Push 60 commands into undoStack to verify limit capping at 50
    for (int i = 0; i < 60; ++i) {
        undoStack.push(new DeleteFramesCommand(&doc, {doc.frameCount() - 1}));
    }

    // QUndoStack::count() reflects current history size capped by undoLimit
    QCOMPARE(undoStack.count(), 50);

    // Test undoing and redoing without texture or memory issues
    QVERIFY(undoStack.canUndo());
    undoStack.undo();
    QCOMPARE(undoStack.count(), 50);
    undoStack.redo();
    QCOMPARE(undoStack.count(), 50);
}

// -----------------------------------------------------------------------------
// AnimationController Tests
// -----------------------------------------------------------------------------

void TestControllers::testAnimationControllerPlayback()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);
    animCtrl.player()->setSequence({0, 1}, 12);

    QCOMPARE(animCtrl.fps(), 12);
    animCtrl.setFps(24);
    QCOMPARE(animCtrl.fps(), 24);

    QSignalSpy spyState(&animCtrl, &AnimationController::playbackStateChanged);

    QVERIFY(!animCtrl.isPlaying());
    animCtrl.play();
    QVERIFY(animCtrl.isPlaying());
    QCOMPARE(spyState.count(), 1);

    animCtrl.pause();
    QVERIFY(!animCtrl.isPlaying());

    animCtrl.togglePlayPause();
    QVERIFY(animCtrl.isPlaying());
    animCtrl.togglePlayPause();
    QVERIFY(!animCtrl.isPlaying());
}

void TestControllers::testAnimationControllerCreateAnimation()
{
    SpriteDocument doc;
    // Add 4 dummy frames
    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(Qt::black);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));
    doc.addSlice(QRect(0, 16, 16, 16));
    doc.addSlice(QRect(16, 16, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    QSignalSpy spyAnim(&animCtrl, &AnimationController::currentAnimationChanged);

    animCtrl.createAnimation(QStringLiteral("walk"), {0, 1, 2}, 15);

    QVERIFY(doc.hasAnimation(QStringLiteral("walk")));
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 2}));
    QCOMPARE(doc.animation(QStringLiteral("walk")).fps, 15);
    QCOMPARE(animCtrl.currentAnimationName(), QStringLiteral("walk"));

    // Undo should remove animation
    undoStack.undo();
    QVERIFY(!doc.hasAnimation(QStringLiteral("walk")));

    // Redo should restore animation
    undoStack.redo();
    QVERIFY(doc.hasAnimation(QStringLiteral("walk")));
}

void TestControllers::testAnimationControllerReverseAnimation()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));
    doc.addSlice(QRect(0, 16, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    animCtrl.createAnimation(QStringLiteral("attack"), {0, 1, 2}, 10);
    QCOMPARE(doc.animation(QStringLiteral("attack")).frameIndices, (QList<int>{0, 1, 2}));

    animCtrl.reverseAnimationOrder();
    QCOMPARE(doc.animation(QStringLiteral("attack")).frameIndices, (QList<int>{2, 1, 0}));

    undoStack.undo();
    QCOMPARE(doc.animation(QStringLiteral("attack")).frameIndices, (QList<int>{0, 1, 2}));
}

void TestControllers::testAnimationControllerRemoveAnimation()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    animCtrl.createAnimation(QStringLiteral("idle"), {0}, 8);
    QVERIFY(doc.hasAnimation(QStringLiteral("idle")));

    animCtrl.removeAnimation(QStringLiteral("idle"));
    QVERIFY(!doc.hasAnimation(QStringLiteral("idle")));

    undoStack.undo();
    QVERIFY(doc.hasAnimation(QStringLiteral("idle")));
}

void TestControllers::testAnimationControllerCurrentSelection()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));

    AnimationController animCtrl(&doc);

    QVERIFY(!animCtrl.hasCurrentAnimation());

    animCtrl.updateCurrentAnimation({0, 1});
    QVERIFY(animCtrl.hasCurrentAnimation());
    QCOMPARE(animCtrl.currentAnimationName(), QStringLiteral("current"));

    animCtrl.removeCurrentAnimation();
    QVERIFY(!animCtrl.hasCurrentAnimation());
}

void TestControllers::testAnimationControllerAutoPlay()
{
    SpriteDocument doc;
    QImage img(48, 48, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));
    doc.addSlice(QRect(32, 0, 16, 16));

    AnimationController animCtrl(&doc);

    // Auto-play default should be true
    QVERIFY(AppConfig::instance().animation().autoPlayOnSelection);

    // 1. Selecting 2 frames automatically starts playback
    animCtrl.updateCurrentAnimation({0, 1});
    QVERIFY(animCtrl.isPlaying());

    // 2. Selecting 1 frame pauses playback
    animCtrl.updateCurrentAnimation({0});
    QVERIFY(!animCtrl.isPlaying());

    // 3. Starting manual play then changing selection preserves playback
    animCtrl.play();
    QVERIFY(animCtrl.isPlaying());
    animCtrl.updateCurrentAnimation({1, 2});
    QVERIFY(animCtrl.isPlaying());
}

// -----------------------------------------------------------------------------
// AtlasViewController Tests
// -----------------------------------------------------------------------------

void TestControllers::testAtlasViewControllerToolMode()
{
    QGraphicsView view;
    SpriteDocument doc;
    AtlasViewController atlasCtrl(&view, &doc);

    QSignalSpy spyTool(&atlasCtrl, &AtlasViewController::toolModeChanged);

    QCOMPARE(atlasCtrl.toolMode(), AtlasViewController::ToolSelect);

    atlasCtrl.setToolMode(AtlasViewController::ToolAddSlice);
    QCOMPARE(atlasCtrl.toolMode(), AtlasViewController::ToolAddSlice);
    QCOMPARE(spyTool.count(), 1);

    atlasCtrl.setToolMode(AtlasViewController::ToolSelect);
    QCOMPARE(atlasCtrl.toolMode(), AtlasViewController::ToolSelect);
    QCOMPARE(spyTool.count(), 2);
}

void TestControllers::testAtlasViewControllerZoom()
{
    QGraphicsView view;
    SpriteDocument doc;
    AtlasViewController atlasCtrl(&view, &doc);

    QSignalSpy spyZoom(&atlasCtrl, &AtlasViewController::zoomChanged);

    atlasCtrl.setZoomFactor(2.0);
    QCOMPARE(atlasCtrl.zoomFactor(), 2.0);
    QCOMPARE(spyZoom.count(), 1);

    // Test clamping limits: min 0.1, max 10.0
    atlasCtrl.setZoomFactor(0.01);
    QCOMPARE(atlasCtrl.zoomFactor(), 0.1);

    atlasCtrl.setZoomFactor(50.0);
    QCOMPARE(atlasCtrl.zoomFactor(), 10.0);

    atlasCtrl.setZoomFactor(1.0);
    atlasCtrl.zoomIn(2.0);
    QCOMPARE(atlasCtrl.zoomFactor(), 2.0);
    atlasCtrl.zoomOut(2.0);
    QCOMPARE(atlasCtrl.zoomFactor(), 1.0);
}

void TestControllers::testAtlasViewControllerBoxSync()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    AtlasViewController atlasCtrl(&view, &doc);

    QCOMPARE(atlasCtrl.boxCount(), 0);

    doc.addSlice(QRect(10, 10, 20, 20));
    doc.addSlice(QRect(40, 10, 20, 20));
    doc.addSlice(QRect(70, 10, 20, 20));

    // framesChanged signal triggers syncAtlasBoxes
    QCOMPARE(atlasCtrl.boxCount(), 3);
}

void TestControllers::testAtlasViewControllerSelection()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(0, 0, 20, 20));
    doc.addSlice(QRect(20, 0, 20, 20));
    doc.addSlice(QRect(40, 0, 20, 20));

    AtlasViewController atlasCtrl(&view, &doc);

    QSignalSpy spySel(&atlasCtrl, &AtlasViewController::selectionChanged);

    atlasCtrl.setSelectedBoxIndices({1});
    QCOMPARE(atlasCtrl.selectedBoxIndices(), QList<int>{1});
    QCOMPARE(spySel.count(), 1);

    atlasCtrl.selectAll();
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{0, 1, 2}));

    atlasCtrl.invertSelection();
    QCOMPARE(atlasCtrl.selectedBoxIndices(), QList<int>());

    atlasCtrl.setSelectedBoxIndices({0});
    atlasCtrl.invertSelection();
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{1, 2}));

    atlasCtrl.clearSelection();
    QCOMPARE(atlasCtrl.selectedBoxIndices(), QList<int>());
}

void TestControllers::testAtlasViewControllerNudge()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20));

    QUndoStack undoStack;
    AtlasViewController atlasCtrl(&view, &doc, &undoStack);

    atlasCtrl.setSelectedBoxIndices({0});

    // Nudge right 5px, down 3px
    atlasCtrl.nudgeSelectedBoxes(5, 3);
    QCOMPARE(doc.box(0).rect, QRect(15, 13, 20, 20));

    // Undo should return box to original pos
    undoStack.undo();
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 20, 20));

    // Redo should apply nudge again
    undoStack.redo();
    QCOMPARE(doc.box(0).rect, QRect(15, 13, 20, 20));
}

void TestControllers::testAtlasViewControllerTrimAndMerge()
{
    QGraphicsView view;
    SpriteDocument doc;
    // Create an atlas with an opaque 10x10 area inside a 30x30 bounding box
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);

    QPainter p(&atlas);
    p.fillRect(5, 5, 10, 10, Qt::red);
    p.fillRect(50, 50, 20, 20, Qt::blue);
    p.end();

    doc.setAtlas(atlas);
    doc.addSlice(QRect(0, 0, 30, 30));    // Box 0: includes the 10x10 red area at (5,5)
    doc.addSlice(QRect(40, 40, 40, 40));  // Box 1: includes the 20x20 blue area at (50,50)

    QUndoStack undoStack;
    AtlasViewController atlasCtrl(&view, &doc, &undoStack);

    // Test Trim on Box 0
    atlasCtrl.setSelectedBoxIndices({0});
    atlasCtrl.trimSelectedSlice(1);
    QCOMPARE(doc.box(0).rect, QRect(5, 5, 10, 10));

    undoStack.undo();
    QCOMPARE(doc.box(0).rect, QRect(0, 0, 30, 30));

    // Test Merge Box 0 and Box 1
    atlasCtrl.setSelectedBoxIndices({0, 1});
    atlasCtrl.mergeSelectedSlices();
    QCOMPARE(doc.frameCount(), 1);

    undoStack.undo();
    QCOMPARE(doc.frameCount(), 2);
}

void TestControllers::testAtlasViewControllerErasePixels()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);

    // Paint an opaque red rectangle at (10, 10, 20, 20)
    QPainter p(&atlas);
    p.fillRect(10, 10, 20, 20, Qt::red);
    p.end();

    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20)); // Frame 0

    QUndoStack undoStack;
    AtlasViewController atlasCtrl(&view, &doc, &undoStack);

    // Select box 0 and erase its pixels
    atlasCtrl.setSelectedBoxIndices({0});
    atlasCtrl.eraseSelectedSlicesPixels();

    // 1. Frame count is now 0
    QCOMPARE(doc.frameCount(), 0);

    // 2. Pixels inside the rect on atlas are now transparent
    QRgb pixelInside = doc.atlas().pixel(15, 15);
    QCOMPARE(qAlpha(pixelInside), 0);

    // 3. Pixel outside the rect remains white
    QRgb pixelOutside = doc.atlas().pixel(5, 5);
    QCOMPARE(qAlpha(pixelOutside), 255);

    // 4. Undo restores frame and original red pixels
    undoStack.undo();
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 20, 20));
    QRgb restoredPixel = doc.atlas().pixel(15, 15);
    QCOMPARE(qAlpha(restoredPixel), 255);
    QCOMPARE(qRed(restoredPixel), 255);

    // 5. Redo erases pixels again
    undoStack.redo();
    QCOMPARE(doc.frameCount(), 0);
    QCOMPARE(qAlpha(doc.atlas().pixel(15, 15)), 0);
}

void TestControllers::testAtlasViewControllerMarqueeSelection()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(200, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    // 4 boxes horizontally
    doc.addSlice(QRect(0, 0, 20, 20));    // 0
    doc.addSlice(QRect(30, 0, 20, 20));   // 1
    doc.addSlice(QRect(60, 0, 20, 20));   // 2
    doc.addSlice(QRect(90, 0, 20, 20));   // 3

    AtlasViewController atlasCtrl(&view, &doc);

    // Initial marquee selecting box 0 and 1
    atlasCtrl.startMarqueeSelection(QPointF(5, 5), Qt::NoModifier);
    atlasCtrl.updateMarqueeSelection(QPointF(45, 15));
    atlasCtrl.endMarqueeSelection();

    QList<int> expected01 = {0, 1};
    QCOMPARE(atlasCtrl.selectedBoxIndices(), expected01);

    // Additive marquee: adds box 2 and 3
    atlasCtrl.startMarqueeSelection(QPointF(55, 5), Qt::ControlModifier);
    atlasCtrl.updateMarqueeSelection(QPointF(95, 15));
    atlasCtrl.endMarqueeSelection();

    QList<int> expectedAll = {0, 1, 2, 3};
    QCOMPARE(atlasCtrl.selectedBoxIndices(), expectedAll);

    // Subtractive marquee: removes box 0
    atlasCtrl.startMarqueeSelection(QPointF(0, 0), Qt::ShiftModifier);
    atlasCtrl.updateMarqueeSelection(QPointF(25, 25));
    atlasCtrl.endMarqueeSelection();

    QList<int> expectedRest = {1, 2, 3};
    QCOMPARE(atlasCtrl.selectedBoxIndices(), expectedRest);
}

void TestControllers::testControllerCrossSyncNoRecursion()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(200, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    doc.addSlice(QRect(0, 0, 20, 20));
    doc.addSlice(QRect(30, 0, 20, 20));
    doc.addSlice(QRect(60, 0, 20, 20));
    doc.addSlice(QRect(90, 0, 20, 20));

    QUndoStack undoStack;
    AtlasViewController atlasCtrl(&view, &doc, &undoStack);
    AnimationController animCtrl(&doc, &undoStack);

    // Wire them identically to MainWindow::setupControllers()
    QObject::connect(&atlasCtrl, &AtlasViewController::selectionChanged,
                     &animCtrl, &AnimationController::updateCurrentAnimation);

    QObject::connect(&animCtrl, &AnimationController::framesSelectedInAnimation,
                     &atlasCtrl, [&atlasCtrl](const QList<int> &indices) {
        if (atlasCtrl.selectedBoxIndices() != indices) {
            atlasCtrl.setSelectedBoxIndices(indices);
        }
    });

    // 1. Selecting on atlas updates animation without infinite recursion
    atlasCtrl.setSelectedBoxIndices({0, 1});
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{0, 1}));
    QVERIFY(doc.hasAnimation(QStringLiteral("current")));
    QCOMPARE(doc.animation(QStringLiteral("current")).frameIndices, (QList<int>{0, 1}));

    // 2. Selecting an explicit animation updates atlas selection
    animCtrl.createAnimation(QStringLiteral("run"), {2, 3}, 12);
    animCtrl.selectAnimation(QStringLiteral("run"));
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{2, 3}));

    // 3. Modifying atlas selection again does not disrupt animation list
    atlasCtrl.setSelectedBoxIndices({1});
    QCOMPARE(doc.animation(QStringLiteral("current")).frameIndices, (QList<int>{1}));
}

void TestControllers::testAtlasViewControllerContextMenuSignals()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20));

    AtlasViewController atlasCtrl(&view, &doc);
    QSignalSpy spyAtlasMenu(&atlasCtrl, &AtlasViewController::atlasContextMenuRequested);

    // Context menu event on empty space (e.g. 80, 80)
    QContextMenuEvent emptyEvent(QContextMenuEvent::Mouse, QPoint(80, 80), view.viewport()->mapToGlobal(QPoint(80, 80)));
    QCoreApplication::sendEvent(view.viewport(), &emptyEvent);

    QCOMPARE(spyAtlasMenu.count(), 1);
    QCOMPARE(spyAtlasMenu.takeFirst().at(0).toPoint(), QPoint(80, 80));
}

void TestControllers::testAtlasViewControllerMultiSelectAndDelete()
{
    QGraphicsView view;
    SpriteDocument doc;
    QUndoStack undoStack;

    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    // Draw distinct colors in 5 regions
    for (int i = 0; i < 5; ++i) {
        QRect r(i * 20, 0, 15, 15);
        for (int y = r.top(); y <= r.bottom(); ++y) {
            QRgb *line = reinterpret_cast<QRgb*>(atlas.scanLine(y));
            for (int x = r.left(); x <= r.right(); ++x) {
                line[x] = qRgba(50 * (i + 1), 0, 0, 255);
            }
        }
    }
    doc.setAtlas(atlas);

    for (int i = 0; i < 5; ++i) {
        doc.addSlice(QRect(i * 20, 0, 15, 15));
    }
    QCOMPARE(doc.frameCount(), 5);

    AtlasViewController atlasCtrl(&view, &doc, &undoStack);

    // 1. Multi-selection does not collapse
    atlasCtrl.setSelectedBoxIndices({1, 3});
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{1, 3}));
    QCOMPARE(doc.selectedFrameIndices(), (QList<int>{1, 3}));

    // 2. Delete selected slices (slices 1 and 3)
    QSignalSpy framesChangedSpy(&doc, &SpriteDocument::framesChanged);
    atlasCtrl.deleteSelectedSlices();

    QCOMPARE(doc.frameCount(), 3);
    QVERIFY(framesChangedSpy.count() >= 1);
    // Remaining boxes should be original 0, 2, 4 (now at 0, 1, 2)
    QCOMPARE(doc.box(0).rect, QRect(0, 0, 15, 15));
    QCOMPARE(doc.box(1).rect, QRect(40, 0, 15, 15));
    QCOMPARE(doc.box(2).rect, QRect(80, 0, 15, 15));

    // 3. Undo restores all 5 frames
    undoStack.undo();
    QCOMPARE(doc.frameCount(), 5);
    QCOMPARE(doc.box(1).rect, QRect(20, 0, 15, 15));
    QCOMPARE(doc.box(3).rect, QRect(60, 0, 15, 15));

    // 4. Redo deletes them again
    undoStack.redo();
    QCOMPARE(doc.frameCount(), 3);

    // 5. Erase pixels for slices 0 and 2 (which correspond to original 0 and 4)
    atlasCtrl.setSelectedBoxIndices({0, 2});
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{0, 2}));

    QSignalSpy atlasChangedSpy(&doc, &SpriteDocument::atlasChanged);
    atlasCtrl.eraseSelectedSlicesPixels();

    // Now only 1 frame remains (original 2, which was at index 1)
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.box(0).rect, QRect(40, 0, 15, 15));
    QVERIFY(atlasChangedSpy.count() >= 1);

    // Verify erased pixels are transparent
    QCOMPARE(qAlpha(doc.atlas().pixel(5, 5)), 0);
    QCOMPARE(qAlpha(doc.atlas().pixel(85, 5)), 0);
    // Non-erased slice still has opaque pixels
    QCOMPARE(qAlpha(doc.atlas().pixel(45, 5)), 255);

    // 6. Undo restores both pixels and frames
    undoStack.undo();
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(qAlpha(doc.atlas().pixel(5, 5)), 255);
    QCOMPARE(qAlpha(doc.atlas().pixel(85, 5)), 255);
}

void TestControllers::testAtlasViewControllerMouseCenteredZoom()
{
    QGraphicsView view;
    view.resize(800, 600);
    view.show();
    QCoreApplication::processEvents();

    SpriteDocument doc;
    QImage atlas(1000, 1000, QImage::Format_ARGB32_Premultiplied);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    AtlasViewController atlasCtrl(&view, &doc);
    atlasCtrl.setZoomFactor(1.0);
    QCoreApplication::processEvents();

    // Zoom centered on a specific viewport point (e.g., 300, 250)
    QPointF mousePos(300.0, 250.0);
    QPointF sceneBefore = view.mapToScene(mousePos.toPoint());

    // Zoom in by factor 1.5
    atlasCtrl.zoomAt(mousePos, 1.5);
    QCOMPARE(atlasCtrl.zoomFactor(), 1.5);

    QPointF sceneAfter = view.mapToScene(mousePos.toPoint());
    qDebug() << "sceneBefore:" << sceneBefore << "sceneAfter:" << sceneAfter
             << "diff:" << (sceneAfter - sceneBefore);
    // The scene point mapped to the cursor must remain stationary (within 2 pixels tolerance)
    QVERIFY(qAbs(sceneAfter.x() - sceneBefore.x()) <= 2.0);
    QVERIFY(qAbs(sceneAfter.y() - sceneBefore.y()) <= 2.0);

    // Zoom out by factor 0.8 at another point
    QPointF mousePos2(150.0, 120.0);
    QPointF sceneBefore2 = view.mapToScene(mousePos2.toPoint());
    atlasCtrl.zoomAt(mousePos2, 0.8);
    QPointF sceneAfter2 = view.mapToScene(mousePos2.toPoint());
    QVERIFY(qAbs(sceneAfter2.x() - sceneBefore2.x()) <= 2.0);
    QVERIFY(qAbs(sceneAfter2.y() - sceneBefore2.y()) <= 2.0);

    // Check zoom limits with zoomAt
    atlasCtrl.setZoomFactor(10.0);
    atlasCtrl.zoomAt(mousePos, 1.5);
    QCOMPARE(atlasCtrl.zoomFactor(), 10.0);

    atlasCtrl.setZoomFactor(0.1);
    atlasCtrl.zoomAt(mousePos, 0.5);
    QCOMPARE(atlasCtrl.zoomFactor(), 0.1);
}

void TestControllers::testI18nKeyTranslations()
{
#ifdef QM_DIR
    QString qmDir = QStringLiteral(QM_DIR);

    // 1. Test French translation
    {
        QTranslator frTranslator;
        bool loaded = frTranslator.load(QStringLiteral("sprite_studio_fr_FR.qm"), qmDir);
        QVERIFY2(loaded, "Failed to load sprite_studio_fr_FR.qm from QM_DIR");

        QCoreApplication::installTranslator(&frTranslator);

        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILE"), QStringLiteral("Fichier"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_OPEN"), QStringLiteral("&Ouvrir"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_SAVE"), QStringLiteral("&Enregistrer"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOL_SELECT"), QStringLiteral("Sélectionner"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_CREATE_ANIM"), QStringLiteral("Créer une animation depuis la sélection"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_TITLE"), QStringLiteral("À propos"));

        QCoreApplication::removeTranslator(&frTranslator);
    }

    // 2. Test English translation
    {
        QTranslator enTranslator;
        bool loaded = enTranslator.load(QStringLiteral("sprite_studio_en_US.qm"), qmDir);
        QVERIFY2(loaded, "Failed to load sprite_studio_en_US.qm from QM_DIR");

        QCoreApplication::installTranslator(&enTranslator);

        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILE"), QStringLiteral("File"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_OPEN"), QStringLiteral("&Open"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_SAVE"), QStringLiteral("&Save"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOL_SELECT"), QStringLiteral("Select & Edit"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_CREATE_ANIM"), QStringLiteral("Create animation from selection"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_TITLE"), QStringLiteral("About"));

        QCoreApplication::removeTranslator(&enTranslator);
    }

    // 3. Test Japanese translation
    {
        QTranslator jaTranslator;
        bool loaded = jaTranslator.load(QStringLiteral("sprite_studio_ja_JA.qm"), qmDir);
        QVERIFY2(loaded, "Failed to load sprite_studio_ja_JA.qm from QM_DIR");

        QCoreApplication::installTranslator(&jaTranslator);

        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILE"), QStringLiteral("ファイル"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_OPEN"), QStringLiteral("開く(&O)"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_SAVE"), QStringLiteral("保存(&S)"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOL_SELECT"), QStringLiteral("選択・編集"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_CREATE_ANIM"), QStringLiteral("選択範囲からアニメーションを作成する"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_TITLE"), QStringLiteral("情報"));

        QCoreApplication::removeTranslator(&jaTranslator);
    }

    // 4. Test Untranslated Key Fallback
    // Missing keys must clearly show visually that they are keys and not final text
    QString untranslated = QCoreApplication::translate("MainWindow", "KEY_UNKNOWN_FEATURE");
    QCOMPARE(untranslated, QStringLiteral("KEY_UNKNOWN_FEATURE"));
    QVERIFY(untranslated.startsWith(QStringLiteral("KEY_")));
#endif
}

void TestControllers::testAtlasBoxItemHandleCosmeticSize()
{
    // Test on micro-sprite (16x16) and normal sprite (64x64)
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.show();

    AtlasBoxItem item16(0, QRect(0, 0, 16, 16), QRect(0, 0, 256, 256));
    scene.addItem(&item16);
    item16.setSelectedBox(true);

    // 1. Test at 1x zoom (scale = 1.0)
    view.resetTransform();
    double size1x = item16.currentHandleSize();
    // 16 * 0.35 = 5.6, so size is capped to 5.6 to prevent overlapping handles on small sprites
    QVERIFY(size1x <= 5.6);
    QVERIFY(size1x >= 1.0);

    // 2. Test at 4x zoom (scale = 4.0)
    view.scale(4.0, 4.0);
    double size4x = item16.currentHandleSize();
    // In scene coords, size4x should be 8.0 / 4.0 = 2.0
    QCOMPARE(size4x, 2.0);

    // 3. Test at 16x zoom (scale = 16.0)
    view.resetTransform();
    view.scale(16.0, 16.0);
    double size16x = item16.currentHandleSize();
    // In scene coords, size16x should be 8.0 / 16.0 = 0.5
    QCOMPARE(size16x, 0.5);

    // 4. Test boundingRect and shape at 16x
    QRectF br = item16.boundingRect();
    QVERIFY(br.contains(item16.boxRect()));
    QPainterPath sp = item16.shape();
    QVERIFY(!sp.isEmpty());
}

void TestControllers::testAtlasViewControllerGroupDrag()
{
    QGraphicsView view;
    SpriteDocument doc;
    QUndoStack undoStack;
    AtlasViewController controller(&view, &doc, &undoStack);

    QImage atlas(200, 200, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    // Add 3 frames
    doc.addSlice(QRect(10, 10, 20, 20));
    doc.addSlice(QRect(40, 10, 20, 20));
    doc.addSlice(QRect(70, 10, 20, 20));
    QCOMPARE(doc.frameCount(), 3);

    // Select all 3 frames
    controller.setSelectedBoxIndices({0, 1, 2});
    QCOMPARE(controller.selectedBoxIndices().size(), 3);

    // Move group by (15, 25)
    controller.moveSelectedBoxes(15, 25);

    QCOMPARE(doc.box(0).rect, QRect(25, 35, 20, 20));
    QCOMPARE(doc.box(1).rect, QRect(55, 35, 20, 20));
    QCOMPARE(doc.box(2).rect, QRect(85, 35, 20, 20));

    // Test Undo
    QVERIFY(undoStack.canUndo());
    undoStack.undo();

    QCOMPARE(doc.box(0).rect, QRect(10, 10, 20, 20));
    QCOMPARE(doc.box(1).rect, QRect(40, 10, 20, 20));
    QCOMPARE(doc.box(2).rect, QRect(70, 10, 20, 20));

    // Test Redo
    QVERIFY(undoStack.canRedo());
    undoStack.redo();

    QCOMPARE(doc.box(0).rect, QRect(25, 35, 20, 20));
    QCOMPARE(doc.box(1).rect, QRect(55, 35, 20, 20));
    QCOMPARE(doc.box(2).rect, QRect(85, 35, 20, 20));

    // Test Boundary Clamping: try to move beyond atlas width (200)
    controller.moveSelectedBoxes(500, 0);
    QVERIFY(doc.box(2).rect.right() <= atlas.rect().right());
    QVERIFY(doc.box(0).rect.left() >= atlas.rect().left());
}

void TestControllers::testAtlasViewControllerContinuousSlice()
{
    QGraphicsView view;
    view.resize(400, 400);
    view.show();

    SpriteDocument doc;
    QUndoStack undoStack;
    AtlasViewController controller(&view, &doc, &undoStack);

    QImage atlas(200, 200, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    // Set ToolAddSlice mode
    controller.setToolMode(AtlasViewController::ToolAddSlice);
    QCOMPARE(controller.toolMode(), AtlasViewController::ToolAddSlice);

    // 1. Draw without Shift -> auto-switches to ToolSelect
    QPoint p1 = view.mapFromScene(QPointF(20, 20));
    QPoint p2 = view.mapFromScene(QPointF(80, 80));

    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, p1);
    QTest::mouseMove(view.viewport(), p2);
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, p2);

    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(controller.toolMode(), AtlasViewController::ToolSelect);

    // 2. Draw WITH Shift -> remains in ToolAddSlice mode for rapid chaining
    controller.setToolMode(AtlasViewController::ToolAddSlice);

    QPoint p3 = view.mapFromScene(QPointF(100, 100));
    QPoint p4 = view.mapFromScene(QPointF(160, 160));

    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::ShiftModifier, p3);
    QTest::mouseMove(view.viewport(), p4);
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::ShiftModifier, p4);

    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(controller.toolMode(), AtlasViewController::ToolAddSlice);
}

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestControllers tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_controllers.moc"
