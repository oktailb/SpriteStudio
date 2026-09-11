#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QSlider>
#include <QMenu>
#include <QUndoStack>
#include <QStyledItemDelegate>
#include <QPainter>
#include <memory>

#include "arrangementmodel.h"
#include "model/spritedocument.h"
#include "animation/animationplayer.h"
#include "controller/projectcontroller.h"
#include "controller/animationcontroller.h"
#include "controller/atlasviewcontroller.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @brief Custom item delegate for drawing visual feedback in the frame list view.
 */
class FrameDelegate : public QStyledItemDelegate
{
public:
    enum HighlightState {
        None,
        Merge,
        InsertLeft,
        InsertRight
    };

    explicit FrameDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void setHighlight(int row, HighlightState state) {
        m_targetRow = row;
        m_state = state;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);

        if (index.row() != m_targetRow || m_state == None) {
            return;
        }

        painter->save();
        if (m_state == Merge) {
            QPen pen(Qt::magenta);
            pen.setWidth(3);
            pen.setStyle(Qt::DotLine);
            QRect rect = option.rect.adjusted(2, 2, -2, -2);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(rect);
        } else if (m_state == InsertLeft || m_state == InsertRight) {
            int xPos = (m_state == InsertLeft) ? option.rect.left() : option.rect.right();
            if (m_state == InsertRight) xPos -= 2;
            painter->setPen(Qt::NoPen);
            painter->setBrush(Qt::black);
            painter->drawRect(xPos, option.rect.top(), 3, option.rect.height());
        }
        painter->restore();
    }

private:
    int             m_targetRow = -1;
    HighlightState  m_state = None;
};

/**
 * @brief The main window orchestrator for the Sprite Studio application.
 *
 * Coordinates UI views with specialized controllers:
 * - ProjectController: File I/O, codecs, recent files, and background removal.
 * - AtlasViewController: Atlas QGraphicsView, zoom/pan, slicing tools, and interactive boxes.
 * - AnimationController: Animation playback, tree widget, preview rendering, and animation CRUD.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    using SliceToolMode = AtlasViewController::SliceToolMode;

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void processFile(const QString &fileName);

    ProjectController* projectController() const { return m_projectController.get(); }
    AtlasViewController* atlasController() const { return m_atlasController.get(); }
    AnimationController* animationController() const { return m_animationController.get(); }

protected:
    void closeEvent(QCloseEvent *e) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    // Menu actions
    void on_actionLicence_triggered();
    void on_actionAbout_triggered();
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionExport_triggered();
    void on_actionExit_triggered();

    // Playback & FPS
    void on_Play_clicked();
    void on_Pause_clicked();
    void on_fps_valueChanged(int fps);
    void zoomSliderChanged(int val);

    // Frame list
    void on_framesList_clicked(const QModelIndex &index);
    void onMergeFrames(int sourceRow, int targetRow);
    void on_framesList_customContextMenuRequested(const QPoint &pos);
    void deleteSelectedFrame();
    void invertSelection();

    // Slicing tools
    void on_btnToolSelect_clicked();
    void on_btnToolAddSlice_clicked();
    void on_btnTrimSlice_clicked();

    // Context menus
    void onAtlasContextMenuRequested(const QPoint &pos);
    void onBoxContextMenuRequested(int index, const QPoint &screenPos);
    void on_animationList_customContextMenuRequested(const QPoint &pos);

    // Image processing
    void removeAtlasBackgroundAndRefresh();

private:
    void setupControllers();
    void setupUIConnections();
    void setupShortcuts();
    void updateRecentFilesMenu();
    void syncFromDocument();
    void populateFrameList(const QList<QPixmap> &frameList, const QList<SpriteBox> &boxList);
    void refreshFrameListDisplay();

    Ui::MainWindow *ui = nullptr;
    ArrangementModel *frameModel = nullptr;
    FrameDelegate *listDelegate = nullptr;

    SpriteDocument *m_document = nullptr;
    QUndoStack *m_undoStack = nullptr;
    AnimationPlayer *m_player = nullptr;

    std::unique_ptr<ProjectController> m_projectController;
    std::unique_ptr<AtlasViewController> m_atlasController;
    std::unique_ptr<AnimationController> m_animationController;

    QMenu *m_recentMenu = nullptr;
    QLabel *statusLabel = nullptr;
    QLabel *zoomLabel = nullptr;
    QSlider *zoomSlider = nullptr;
    QProgressBar *progressBar = nullptr;
};

#endif // MAINWINDOW_H
