#include "arrangementmodel.h"
#include <QDebug>
#include <QIODevice>

ArrangementModel::ArrangementModel(QObject *parent)
    : QStandardItemModel(parent)
{
}

Qt::ItemFlags ArrangementModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QStandardItemModel::flags(index);
    if (index.isValid()) {
        // IMPORTANT : ItemIsDropEnabled permet de lâcher SUR l'item
        return defaultFlags | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    }
    // Si index invalide (espace vide), on autorise le drop pour la réorganisation
    return defaultFlags | Qt::ItemIsDropEnabled;
}

QMimeData *ArrangementModel::mimeData(const QModelIndexList &indexes) const
{
    // On utilise l'implémentation par défaut pour les données standard
    QMimeData *mimeData = QStandardItemModel::mimeData(indexes);

    // On ajoute une données perso pour identifier facilement la ligne source
    if (!indexes.isEmpty()) {
        QByteArray encodedData;
        QDataStream stream(&encodedData, QIODevice::WriteOnly);
        // On prend juste la première ligne sélectionnée pour simplifier
        stream << indexes.first().row();
        mimeData->setData("application/x-sprite-row", encodedData);
    }
    return mimeData;
}

bool ArrangementModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                    int row, int column, const QModelIndex &parent)
{
    // Vérifions si c'est une fusion (Drop SUR un item existant)
    // Si 'parent' est valide, cela signifie qu'on a lâché sur cet item
    if (parent.isValid() && data->hasFormat("application/x-sprite-row")) {

        QByteArray encodedData = data->data("application/x-sprite-row");
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        int sourceRow;
        stream >> sourceRow;

        int targetRow = parent.row();

        // On évite de fusionner sur soi-même
        if (sourceRow != targetRow) {
            qDebug() << "Fusion demandée : Source" << sourceRow << "vers Cible" << targetRow;
            emit mergeRequested(sourceRow, targetRow);
            return true; // Drop accepté et traité
        }
    }

    // Sinon, comportement standard (réorganisation)
    return QStandardItemModel::dropMimeData(data, action, row, column, parent);
}
