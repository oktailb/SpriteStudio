#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <qgraphicsitem.h>
#include <qstandarditemmodel.h>
#include "extractor.h"
#include "arrangementmodel.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void timerEvent(QTimerEvent *event) override;
    void closeEvent(QCloseEvent *e) override;

private slots:

    void on_actionLicence_triggered();

    void on_actionAbout_triggered();

    void on_actionOpen_triggered();

    void on_actionExit_triggered();

    void on_fps_valueChanged(int arg1);

    void on_alphaThreshold_valueChanged(int threshold);

    void on_verticalTolerance_valueChanged(int verticalTolerance);

    void on_framesList_clicked(const QModelIndex &index);

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
    QGraphicsRectItem                   *boundingBoxHighlighter;
    void populateFrameList(const QList<QPixmap> &frameList, const QList<Extractor::Box> &boxList);
    void processFile(const QString &fileName);
};
#endif // MAINWINDOW_H
