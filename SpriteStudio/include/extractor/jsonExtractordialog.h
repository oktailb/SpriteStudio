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
    QImage::Format imageFormat();
    QString imageFormatAsString();
    QList<QString> selectedAnimations() const;
    AtlasStrategy selectedStrategy() const;

private slots:
    void on_animations_itemSelectionChanged();
    void on_replaceExistingAtlas_checkStateChanged(const Qt::CheckState &state);
    void on_atlasSaveStrategy_currentIndexChanged(int index);

private:
    Ui::jsonExtractorDialog * ui;
    Extractor *               m_in;
    QString                   m_baseName;
    ExportOptions             m_opts;
    QList<QString>            m_selectedAnimations;
    AtlasStrategy             m_selectedStrategy;
};

#endif // JSONEXTRACTORDIALOG_H
