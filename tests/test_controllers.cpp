#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QGraphicsView>
#include <QUndoStack>
#include <QContextMenuEvent>

#include "model/spritedocument.h"
#include "controller/projectcontroller.h"
#include "controller/animationcontroller.h"
#include "controller/atlasviewcontroller.h"
#include "animation/animationplayer.h"
#include "config/appconfig.h"
#include "commands/commands.h"

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

    // AtlasViewController tests
    void testAtlasViewControllerToolMode();
    void testAtlasViewControllerZoom();
    void testAtlasViewControllerBoxSync();
    void testAtlasViewControllerSelection();
    void testAtlasViewControllerNudge();
    void testAtlasViewControllerTrimAndMerge();
    void testAtlasViewControllerMarqueeSelection();
    void testAtlasViewControllerContextMenuSignals();
    void testControllerCrossSyncNoRecursion();

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

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestControllers tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_controllers.moc"
