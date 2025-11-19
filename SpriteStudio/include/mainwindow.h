#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <qstandarditemmodel.h>
#include "extractor.h"

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

private:
    Ui::MainWindow *ui;

    double                              timestamp;

    // UI related
    int                                 timerId;
    bool                                ready;
    QList<QPixmap>                      frames;
    QPixmap                             frame;
    QStandardItemModel                  *frameModel;
    QList<Extractor::Box>               frameBoxes;

    void populateFrameList(const QList<QPixmap> &frameList, const QList<Extractor::Box> &boxList);
};
#endif // MAINWINDOW_H
