#pragma once

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QJSValue>
#include <memory>
#include "qbackendabstractconnection.h"

class BackendModelPrivate;

class QBackendModel : public QAbstractListModel
{
    friend class BackendModelPrivate;

public:
    QBackendModel(QBackendAbstractConnection *connection, QByteArray identifier, const QJsonObject &type, QObject *parent = nullptr);
    virtual ~QBackendModel();

    virtual const QMetaObject *metaObject() const override;
    virtual int qt_metacall(QMetaObject::Call c, int id, void **argv) override;

protected:
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex&) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    BackendModelPrivate *d;
    QMetaObject *m_metaObject = nullptr;
};

Q_DECLARE_METATYPE(QBackendModel*)
