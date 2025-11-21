#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <qgraphicsitem.h>
#include <qstandarditemmodel.h>
#include <QStyledItemDelegate>
#include <QPainter>
#include "extractor.h"
#include "arrangementmodel.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// Délégué personnalisé pour dessiner le contour de fusion
class FrameDelegate : public QStyledItemDelegate
{
public:
    // On définit les états possibles
    enum HighlightState {
        None,
        Merge,       // Au centre (Fusion)
        InsertLeft,  // Bord gauche (Insertion avant)
        InsertRight  // Bord droit (Insertion après)
    };

    explicit FrameDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    // Une seule fonction pour tout configurer
    void setHighlight(int row, HighlightState state) {
        m_targetRow = row;
        m_state = state;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);

        // Si on n'est pas sur la ligne concernée, on ne fait rien
        if (index.row() != m_targetRow || m_state == None) {
            return;
        }

        painter->save();

        if (m_state == Merge) {
            // --- DESSIN FUSION (Cadre Magenta) ---
            QPen pen(Qt::magenta);
            pen.setWidth(3);
            pen.setStyle(Qt::DotLine);
            QRect rect = option.rect.adjusted(2, 2, -2, -2);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(rect);
        }
        else if (m_state == InsertLeft || m_state == InsertRight) {
            // --- DESSIN INSERTION (Barre Noire) ---
            // On dessine une ligne verticale noire épaisse sur le côté approprié
            int xPos = (m_state == InsertLeft) ? option.rect.left() : option.rect.right();

            // On ajuste pour que la ligne soit bien visible
            if (m_state == InsertRight) xPos -= 2;

            painter->setPen(Qt::NoPen);
            painter->setBrush(Qt::black);
            // Une barre de 3px de large, sur toute la hauteur de l'item
            painter->drawRect(xPos, option.rect.top(), 3, option.rect.height());
        }

        painter->restore();
    }

private:
    int m_targetRow = -1;
    HighlightState m_state = None;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void timerEvent(QTimerEvent *event) override;
    void closeEvent(QCloseEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:

    void on_actionLicence_triggered();

    void on_actionAbout_triggered();

    void on_actionOpen_triggered();

    void on_actionExit_triggered();

    void on_fps_valueChanged(int arg1);

    void on_alphaThreshold_valueChanged(int threshold);

    void on_verticalTolerance_valueChanged(int verticalTolerance);

    void on_framesList_clicked(const QModelIndex &index);

    void onMergeFrames(int sourceRow, int targetRow);

    void on_framesList_customContextMenuRequested(const QPoint &pos);

    void deleteSelectedFrame();

    void invertSelection();

    void updateAnimation();

    void reverseSelectedFramesOrder();

    void on_actionExport_triggered();

    void on_Play_clicked();

    void on_Pause_clicked();

    void onAtlasContextMenuRequested(const QPoint &pos);

    void removeAtlasBackground();

private:
    Ui::MainWindow *ui;
    ArrangementModel                    *frameModel;
    double                              timestamp;

    // UI related
    int                                 timerId;
    bool                                ready;
    Extractor                           *extractor;
    QString                             currentFilePath;
    QGraphicsRectItem                   *boundingBoxHighlighter= nullptr;
    QGraphicsRectItem                   *mergeHighlighter = nullptr;
    FrameDelegate                       *listDelegate;
    QTimer                              *animationTimer;
    int                                 currentAnimationFrameIndex;
    QList<int>                          selectedFrameRows;

    void populateFrameList(const QList<QPixmap> &frameList, const QList<Extractor::Box> &boxList);
    void processFile(const QString &fileName);
    void setupGraphicsView(const QPixmap &pixmap);
    void setMergeHighlight(const QModelIndex &index, bool show);
    void clearMergeHighlight();
    void deleteFrame(int row);
    void startAnimation();
    void stopAnimation();
};
#endif // MAINWINDOW_H
