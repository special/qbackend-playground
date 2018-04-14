#pragma once

#include <QAbstractListModel>
#include <QJSValue>
#include <QUuid>

#include "qbackendabstractconnection.h"

class QBackendJsonListModelProxy;

class QBackendJsonListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QByteArray identifier READ identifier WRITE setIdentifier NOTIFY identifierChanged)
    Q_PROPERTY(QStringList roles READ roles WRITE setRoles NOTIFY roleNamesChanged)
    Q_PROPERTY(QBackendAbstractConnection* connection READ connection WRITE setConnection NOTIFY connectionChanged)
public:
    QBackendJsonListModel(QObject *parent = 0);

    QByteArray identifier() const;
    void setIdentifier(const QByteArray& identifier);

    QStringList roles() const;
    void setRoles(const QStringList & roleNames);

    QBackendAbstractConnection* connection() const;
    void setConnection(QBackendAbstractConnection* connection);

    Q_INVOKABLE void invokeMethod(const QString& method, const QJSValue& data = QJSValue());

signals:
    void identifierChanged();
    void roleNamesChanged();
    void connectionChanged();

protected:
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex&) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    QByteArray m_identifier;
    QStringList m_flatRoleNames;
    QHash<int, QByteArray> m_roleNames;
    QVector<QUuid> m_idMap;
    QVector<QMap<QByteArray, QVariant>> m_data;
    QBackendJsonListModelProxy *m_proxy = nullptr;
    QBackendAbstractConnection *m_connection = nullptr;

    void subscribeIfReady();

    friend class QBackendJsonListModelProxy;
    void doReset(const QJsonDocument& document);
    void doSet(const QUuid& uuid, const QJsonObject& object, bool shouldEmit = false);
    void doRemove(const QUuid& uuid);
};

