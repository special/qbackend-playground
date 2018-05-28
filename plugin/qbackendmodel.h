#pragma once

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QJSValue>
#include <memory>

#include "qbackendabstractconnection.h"

class QBackendInternalModel;
class QBackendModelProxy;

// QBackendModel is the QML-exposed item model type, acting as a sort/filter
// proxy model around a potentially-shared internal model.
class QBackendModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QByteArray identifier READ identifier WRITE setIdentifier NOTIFY identifierChanged)
    Q_PROPERTY(QStringList roles READ roles WRITE setRoles NOTIFY roleNamesChanged)
    Q_PROPERTY(QBackendAbstractConnection* connection READ connection WRITE setConnection NOTIFY connectionChanged)

public:
    QBackendModel(QObject *parent = nullptr);
    virtual ~QBackendModel();

    QByteArray identifier() const;
    void setIdentifier(const QByteArray& identifier);

    QStringList roles() const;
    void setRoles(const QStringList &roleNames);

    QBackendAbstractConnection* connection() const;
    void setConnection(QBackendAbstractConnection* connection);

    Q_INVOKABLE void invokeMethod(const QString& method, const QJSValue& data = QJSValue());

signals:
    void identifierChanged();
    void roleNamesChanged();
    void connectionChanged();

private:
    QByteArray m_identifier;
    QBackendAbstractConnection *m_connection = nullptr;
    std::shared_ptr<QBackendInternalModel> m_model;
    QStringList m_roleNames;

    void subscribeIfReady();
};

// QBackendInternalModel stores and represents the backend model data,
// and can be shared by multiple instances of QBackendModel. It is not
// directly exposed to user code.
class QBackendInternalModel : public QAbstractListModel
{
    Q_OBJECT

public:
    virtual ~QBackendInternalModel();

    static std::shared_ptr<QBackendInternalModel> instance(QBackendAbstractConnection *connection, QByteArray identifier);

    void setRoleNames(QStringList names);

protected:
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex&) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    friend class QBackendModelProxy;
    void doReset(const QJsonDocument &document);
    // XXX set and remove

private:
    static QHash<std::pair<QBackendAbstractConnection*,QString>, std::weak_ptr<QBackendInternalModel>> m_instances;

    QByteArray m_identifier;
    QBackendAbstractConnection *m_connection = nullptr;
    QBackendModelProxy *m_proxy = nullptr;
    QHash<int, QByteArray> m_roleNames;

    // XXX better data structure for this..?
    QVector<QMap<QByteArray,QVariant>> m_data;

    QBackendInternalModel(QBackendAbstractConnection *connection, QByteArray identifier);
};

