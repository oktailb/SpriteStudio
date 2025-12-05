#ifndef JSONEXTRACTORDIALOG_H
#define JSONEXTRACTORDIALOG_H

#include <QDialog>
#include "extractor/extractor.h"
#include "extractor/export.h"

namespace Ui {
class jsonExtractorDialog;
}

class jsonExtractorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit jsonExtractorDialog(Extractor* in, QString baseName, QWidget *parent = nullptr);
    ~jsonExtractorDialog();

    ExportOptions getOpts() const;
    bool replaceAtlas();
    Format selectedFormat();
    QString imageFormat();

    QList<QString> selectedAnimations() const;

  private slots:
    void on_animations_itemSelectionChanged();

  private:
    Ui::jsonExtractorDialog * ui;
    Extractor *               m_in;
    QString                   m_baseName;
    ExportOptions             m_opts;
    QList<QString>            m_selectedAnimations;
};

#endif // JSONEXTRACTORDIALOG_H
