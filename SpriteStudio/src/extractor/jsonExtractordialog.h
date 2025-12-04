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

private:
    Ui::jsonExtractorDialog *ui;
    Extractor * m_in;
    QString  m_baseName;
    ExportOptions opts;
};

#endif // JSONEXTRACTORDIALOG_H
