#ifndef JSONEXTRACTORDIALOG_H
#define JSONEXTRACTORDIALOG_H

#include <QDialog>
#include "extractor.h"

namespace Ui {
class jsonExtractorDialog;
}

class jsonExtractorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit jsonExtractorDialog(QWidget *parent = nullptr);
    ~jsonExtractorDialog();

private:
    Ui::jsonExtractorDialog *ui;
    Extractor * m_in;
};

#endif // JSONEXTRACTORDIALOG_H
