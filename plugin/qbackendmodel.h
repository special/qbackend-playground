#pragma once

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QJSValue>
#include <memory>

class QBackendConnection;
class BackendModelPrivate;

class QBackendModel : public QAbstractListModel
{
    friend class BackendModelPrivate;

public:
    QBackendModel(QBackendConnection *connection, QByteArray identifier, const QJsonObject &type, QObject *parent = nullptr);
    virtual ~QBackendModel();

    virtual const QMetaObject *metaObject() const override;
    virtual int qt_metacall(QMetaObject::Call c, int id, void **argv) override;

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex&) const override;
    QVariant data(const QModelIndex &index, int role) const override;

protected:
    QBackendModel(QBackendConnection *connection, QMetaObject *type);

private:
    BackendModelPrivate *d;
    QMetaObject *m_metaObject = nullptr;
};

Q_DECLARE_METATYPE(QBackendModel*)
