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
    explicit FrameDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    // Méthode pour définir quelle ligne doit être surlignée
    void setMergeTarget(int row) {
        m_targetRow = row;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        // 1. Dessin standard (texte, icône, sélection bleue normale)
        QStyledItemDelegate::paint(painter, option, index);

        // 2. Dessin personnalisé pour la FUSION
        // Si c'est la ligne ciblée par le drag & drop
        if (index.row() == m_targetRow && m_targetRow != -1) {
            painter->save();

            // Configuration du stylo Magenta Pointillé
            QPen pen(Qt::magenta);
            pen.setWidth(3);
            pen.setStyle(Qt::DotLine);

            // On réduit légèrement le rectangle pour qu'il soit bien à l'intérieur
            QRect rect = option.rect.adjusted(2, 2, -2, -2);

            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(rect);

            painter->restore();
        }
    }

private:
    int m_targetRow = -1; // -1 signifie "pas de cible"
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

private:
    Ui::MainWindow *ui;
    ArrangementModel                    *frameModel;
    QList<Extractor::Box>               frameBoxes;
    double                              timestamp;

    // UI related
    int                                 timerId;
    bool                                ready;
    QList<QPixmap>                      frames;
    QPixmap                             frame;
    QString                             currentFilePath;
    QGraphicsRectItem                   *boundingBoxHighlighter= nullptr;
    QGraphicsRectItem                   *mergeHighlighter = nullptr;
    FrameDelegate                       *listDelegate;

    void populateFrameList(const QList<QPixmap> &frameList, const QList<Extractor::Box> &boxList);
    void processFile(const QString &fileName);
    void setupGraphicsView(const QPixmap &pixmap);
    void setMergeHighlight(const QModelIndex &index, bool show);
    void clearMergeHighlight();
};
#endif // MAINWINDOW_H
