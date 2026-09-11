#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include "include/aboutdialog.h"
#include "include/extractor/extractorregistry.h"
#include "include/commands/commands.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

static QString readTextFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString("Error: Cannot open file %1").arg(filePath);
    }
    QTextStream in(&file);
    return in.readAll();
}

void MainWindow::on_actionLicence_triggered()
{
    QString licenseText = readTextFile(":/text/license.txt");
    QDialog dialog;
    dialog.setWindowTitle(tr("Licence"));
    dialog.setMinimumSize(600, 400);
    QTextEdit *textEdit = new QTextEdit(&dialog);
    textEdit->setPlainText(licenseText);
    textEdit->setReadOnly(true);
    QPushButton *closeButton = new QPushButton(tr("Close"), &dialog);
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(textEdit);
    layout->addWidget(closeButton);
    dialog.exec();
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}

void MainWindow::on_actionOpen_triggered()
{
    const QString title = tr("_open_file");
    const QString formats = ExtractorRegistry::instance().openFilterString();
    QString fileName = QFileDialog::getOpenFileName(this, title, "", formats);
    if (!fileName.isEmpty()) {
        processFile(fileName);
    }
}

void MainWindow::on_actionSave_triggered()
{
    if (!m_projectController || !m_document || m_document->isEmpty()) {
        QMessageBox::warning(this, tr("Save"), tr("Nothing to save."));
        return;
    }

    QString currentPath = m_projectController->currentFilePath();
    if (currentPath.isEmpty()) {
        on_actionExport_triggered();
        return;
    }

    QString errorMsg;
    if (!m_projectController->save(currentPath, &errorMsg)) {
        QMessageBox::critical(this, tr("Save Error"), errorMsg);
    }
}

void MainWindow::on_actionExport_triggered()
{
    if (!m_projectController || !m_document || m_document->isEmpty()) {
        QMessageBox::warning(this, tr("Export"), tr("Nothing to export."));
        return;
    }

    QString initialDir = m_projectController->currentFilePath().isEmpty()
        ? QDir::homePath()
        : QFileInfo(m_projectController->currentFilePath()).absolutePath();

    const QString filter = ExtractorRegistry::instance().saveFilterString();
    QString selectedFilter;
    QString selectedFile = QFileDialog::getSaveFileName(this, tr("Export Atlas"), initialDir, filter, &selectedFilter);

    if (selectedFile.isEmpty()) return;

    ExportOptions options;
    QString errorMsg;
    if (!m_projectController->exportData(selectedFile, options, &errorMsg)) {
        QMessageBox::critical(this, tr("Export Error"), errorMsg);
    }
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::zoomSliderChanged(int val)
{
    if (m_atlasController) {
        m_atlasController->setZoomFactor(static_cast<double>(val) / 100.0);
    }
}

void MainWindow::onMergeFrames(int sourceRow, int targetRow)
{
    if (!m_document) return;
    if (m_undoStack) {
        m_undoStack->push(new MergeFramesCommand(m_document, sourceRow, targetRow));
    } else {
        m_document->mergeFrames(sourceRow, targetRow);
    }
}

void MainWindow::on_framesList_customContextMenuRequested(const QPoint &pos)
{
    QModelIndex clickedIdx = ui->framesList->indexAt(pos);
    if (clickedIdx.isValid() && ui->framesList->selectionModel()) {
        if (!ui->framesList->selectionModel()->isSelected(clickedIdx)) {
            ui->framesList->selectionModel()->select(clickedIdx, QItemSelectionModel::ClearAndSelect);
        }
    }

    QMenu menu(this);

    QAction *createAnimAction = menu.addAction(tr("Create animation from selection"));
    createAnimAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(createAnimAction, &QAction::triggered, this, [this]() {
        if (m_animationController && m_document) {
            m_animationController->createAnimationFromSelection(m_document->selectedFrameIndices());
        }
    });

    menu.addSeparator();

    QAction *deleteFramesAction = menu.addAction(tr("Delete Selected Frames\tDel"));
    deleteFramesAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(deleteFramesAction, &QAction::triggered, this, &MainWindow::deleteSelectedFrame);

    QAction *eraseFramesAction = menu.addAction(tr("Erase Pixels && Delete Frames\tShift+Del"));
    eraseFramesAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(eraseFramesAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) {
            m_atlasController->eraseSelectedSlicesPixels();
        }
    });

    menu.addSeparator();

    QAction *invertAction = menu.addAction(tr("Invert Selection"));
    invertAction->setEnabled(m_document && m_document->frameCount() > 0);
    connect(invertAction, &QAction::triggered, this, &MainWindow::invertSelection);

    menu.exec(ui->framesList->mapToGlobal(pos));
}

void MainWindow::deleteSelectedFrame()
{
    if (m_atlasController) {
        m_atlasController->deleteSelectedSlices();
    }
}

void MainWindow::invertSelection()
{
    if (m_atlasController) {
        m_atlasController->invertSelection();
    }
}
