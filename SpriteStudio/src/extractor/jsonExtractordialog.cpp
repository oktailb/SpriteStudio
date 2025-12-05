#include "extractor/jsonExtractordialog.h"
#include "ui_jsonExtractordialog.h"
#include <QGraphicsPixmapItem>

jsonExtractorDialog::jsonExtractorDialog(Extractor* in, QString baseName, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::jsonExtractorDialog)
    , m_in(in)
    , m_baseName(baseName)
{
    ui->setupUi(this);

    ui->baseName->setText(m_baseName);
    ui->preview->setRenderHint(QPainter::Antialiasing, true);
    ui->preview->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    ui->preview->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    ui->preview->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    ui->preview->fitInView(ui->preview->sceneRect(), Qt::KeepAspectRatio);
    QGraphicsScene *sceneLayers = ui->preview->scene();
    if (!sceneLayers) {
        sceneLayers = new QGraphicsScene(this);
        ui->preview->setScene(sceneLayers);
    }
    sceneLayers->clear();

    QGraphicsPixmapItem *item = sceneLayers->addPixmap(QPixmap::fromImage(in->m_atlas));
    sceneLayers->setSceneRect(in->m_atlas.rect());
    ui->preview->fitInView(item, Qt::KeepAspectRatio);
    for(auto anim = in->m_animationsData.begin() ; anim != in->m_animationsData.end() ; anim++) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setData(Qt::DisplayRole, anim.key() + " (" + QString::number(anim.value().frameIndices.count()) + " frames)");
        QImage deco = in->m_frames[anim.value().frameIndices.first()].toImage();
        item->setData(Qt::DecorationRole, deco.scaled(60, 64, Qt::KeepAspectRatio));
        item->setData(Qt::UserRole, anim.key());
        ui->animations->addItem(item);
    }
    ui->targetFormat->insertItem(0, "Texture Packer", Format::FORMAT_TEXTUREPACKER_JSON);
    ui->targetFormat->insertItem(0, "Phaser", Format::FORMAT_PHASER_JSON);
    ui->targetFormat->insertItem(0, "Aseprite", Format::FORMAT_ASEPRITE_JSON);
}

jsonExtractorDialog::~jsonExtractorDialog()
{
    delete ui;
}

ExportOptions jsonExtractorDialog::getOpts() const
{
    return m_opts;
}

bool jsonExtractorDialog::replaceAtlas()
{
    return ui->replaceExistingAtlas->isChecked();
}

Format jsonExtractorDialog::selectedFormat()
{
    return (Format)ui->targetFormat->currentData().toInt();
}

void jsonExtractorDialog::on_animations_itemSelectionChanged()
{
  QList<QListWidgetItem*> selection = ui->animations->selectedItems();
  m_selectedAnimations.clear();
  for (auto item : selection) {
      m_selectedAnimations.push_back(item->data(Qt::UserRole).toString());
    }
}

QList<QString> jsonExtractorDialog::selectedAnimations() const
{
  return m_selectedAnimations;
}

