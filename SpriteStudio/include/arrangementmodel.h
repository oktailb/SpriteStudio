#ifndef ARRANGEMENTMODEL_H
#define ARRANGEMENTMODEL_H

#include <QStandardItemModel>
#include <QObject>
#include <QMimeData>

class ArrangementModel : public QStandardItemModel
{
    Q_OBJECT

public:
    explicit ArrangementModel(QObject *parent = nullptr);

    // Configuration des flags pour autoriser le drop SUR un item
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Pour transporter simplement le numéro de ligne source
    QMimeData *mimeData(const QModelIndexList &indexes) const override;

    // Gestion du lâcher (Drop)
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent) override;

signals:
    // Signal émis quand une fusion est demandée
    void mergeRequested(int sourceRow, int targetRow);
};

#endif // ARRANGEMENTMODEL_H
