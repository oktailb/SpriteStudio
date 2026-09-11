#include "extractor/jsonExtractordialog.h"
#include "ui_jsonExtractordialog.h"
#include <QGraphicsPixmapItem>
#include <QMetaEnum>
#include <QStandardItemModel>

void setupImageFormatComboBox(QComboBox *comboBox) {
    const QMetaObject &metaObject = QImage::staticMetaObject;
    int enumIndex = metaObject.indexOfEnumerator("Format");
    QMetaEnum metaEnum = metaObject.enumerator(enumIndex);

    QStandardItemModel *model = new QStandardItemModel(comboBox);

    for (int i = 0; i < metaEnum.keyCount(); ++i) {
        QString key = metaEnum.key(i);
        if (key.contains("_")) {
            key = key.split("_")[1];
            int value = metaEnum.value(i);
            QStandardItem *item = new QStandardItem(key);
            item->setData(value, Qt::UserRole);
            model->appendRow(item);
        }
    }

    comboBox->setModel(model);
}

jsonExtractorDialog::jsonExtractorDialog(const SpriteDocument &doc, const QString &baseName, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::jsonExtractorDialog)
    , m_baseName(baseName)
    , m_selectedStrategy(AtlasStrategy::ATLASSTRATEGY_ORIGINAL_ATLAS)
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

    if (!doc.atlas().isNull()) {
        QGraphicsPixmapItem *item = sceneLayers->addPixmap(QPixmap::fromImage(doc.atlas()));
        sceneLayers->setSceneRect(doc.atlas().rect());
        ui->preview->fitInView(item, Qt::KeepAspectRatio);
    }

    auto animMap = doc.animations();
    if (animMap.isEmpty() && doc.frameCount() > 0) {
        SpriteAnimation defaultAnim;
        defaultAnim.name = QStringLiteral("default");
        defaultAnim.fps = 12;
        for (int i = 0; i < doc.frameCount(); ++i) {
            defaultAnim.frameIndices.append(i);
        }
        animMap.insert(defaultAnim.name, defaultAnim);
    }

    for (auto anim = animMap.begin(); anim != animMap.end(); ++anim) {
        QListWidgetItem *listItem = new QListWidgetItem();
        listItem->setData(Qt::DisplayRole, anim.key() + " (" + QString::number(anim.value().frameIndices.count()) + " frames)");
        if (!anim.value().frameIndices.isEmpty()) {
            int firstIdx = anim.value().frameIndices.first();
            if (firstIdx >= 0 && firstIdx < doc.frameCount()) {
                QImage deco = doc.frame(firstIdx).toImage();
                listItem->setData(Qt::DecorationRole, deco.scaled(60, 64, Qt::KeepAspectRatio));
            }
        }
        listItem->setData(Qt::UserRole, anim.key());
        ui->animations->addItem(listItem);
    }

    ui->animations->selectAll();
    for (int i = 0; i < ui->animations->count(); ++i) {
        m_selectedAnimations.append(ui->animations->item(i)->data(Qt::UserRole).toString());
    }

    ui->targetFormat->addItem("Texture Packer", Format::FORMAT_TEXTUREPACKER_JSON);
    ui->targetFormat->addItem("Phaser", Format::FORMAT_PHASER_JSON);
    ui->targetFormat->addItem("Aseprite", Format::FORMAT_ASEPRITE_JSON);

    setupImageFormatComboBox(ui->imageFormats);
    int currentIndex = ui->imageFormats->findData(doc.atlas().format(), Qt::UserRole, Qt::MatchExactly);
    ui->imageFormats->setCurrentIndex(currentIndex);

    ui->atlasSaveStrategy->addItem(tr("Use original Atlas"), AtlasStrategy::ATLASSTRATEGY_ORIGINAL_ATLAS);
    ui->atlasSaveStrategy->addItem(tr("Generate same minimal Atlas for all animations"), AtlasStrategy::ATLASSTRATEGY_ONE_ATLAS_FOR_ALL_ANIMATIONS);
    ui->atlasSaveStrategy->addItem(tr("Generate one Atlas per animation"), AtlasStrategy::ATLASSTRATEGY_ONE_ATLAS_PER_ANIMATION);
    m_selectedStrategy = (AtlasStrategy)ui->atlasSaveStrategy->currentData().toInt();

    ui->replaceExistingAtlas->setChecked(true);
    ui->atlasSaveStrategy->setEnabled(true);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
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

QString jsonExtractorDialog::imageFormatAsString()
{
    return ui->imageFormats->currentText();
}

QImage::Format jsonExtractorDialog::imageFormat()
{
    return (QImage::Format)ui->imageFormats->currentData(Qt::UserRole).toInt();
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

void jsonExtractorDialog::on_replaceExistingAtlas_checkStateChanged(const Qt::CheckState &state)
{
    ui->atlasSaveStrategy->setEnabled((state == Qt::Checked) ? true : false);
}

void jsonExtractorDialog::on_atlasSaveStrategy_currentIndexChanged(int index)
{
    m_selectedStrategy = (AtlasStrategy)ui->atlasSaveStrategy->itemData(index).toInt();
}

AtlasStrategy jsonExtractorDialog::selectedStrategy() const
{
    return m_selectedStrategy;
}
