#include "jsonExtractordialog.h"
#include "ui_jsonExtractordialog.h"

jsonExtractorDialog::jsonExtractorDialog(QWidget *parent, Extractor* in)
    : QDialog(parent)
    , ui(new Ui::jsonExtractorDialog)
    , m_in(in)
{
    ui->setupUi(this);

}

jsonExtractorDialog::~jsonExtractorDialog()
{
    delete ui;
}
