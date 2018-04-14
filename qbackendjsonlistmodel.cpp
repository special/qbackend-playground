#include <iostream>
#include <QDebug>
#include <QQmlEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>

#include "qbackendabstractconnection.h"
#include "qbackendjsonlistmodel.h"

Q_LOGGING_CATEGORY(lcListModel, "backend.listmodel")

QBackendJsonListModel::QBackendJsonListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QBackendAbstractConnection* QBackendJsonListModel::connection() const
{
    return m_connection;
}

// ### error on componentComplete if not set
void QBackendJsonListModel::setConnection(QBackendAbstractConnection* connection)
{
    if (connection == m_connection) {
        return;
    }

    m_connection = connection;
    if (!m_identifier.isEmpty()) {
        subscribeIfReady();
    }
    emit connectionChanged();
}

QStringList QBackendJsonListModel::roles() const
{
    return m_flatRoleNames;
}

// ### error on componentComplete if not set
void QBackendJsonListModel::setRoles(const QStringList &roleNames)
{
    if (roleNames == m_flatRoleNames) {
        return;
    }

    m_flatRoleNames = roleNames;
    if (!m_identifier.isEmpty()) {
        subscribeIfReady();
    }
    emit roleNamesChanged();
}

QByteArray QBackendJsonListModel::identifier() const
{
    return m_identifier;
}

class QBackendJsonListModelProxy : public QBackendRemoteObject
{
public:
    QBackendJsonListModelProxy(QBackendJsonListModel* model);
    void objectFound(const QJsonDocument& document) override;
    void methodInvoked(const QByteArray& method, const QJsonDocument& document) override;

private:
    QBackendJsonListModel *m_model = nullptr;
};

QBackendJsonListModelProxy::QBackendJsonListModelProxy(QBackendJsonListModel* model)
    : m_model(model)
{
}

void QBackendJsonListModelProxy::objectFound(const QJsonDocument& document)
{
    m_model->doReset(document);
}

void QBackendJsonListModel::doReset(const QJsonDocument& document)
{
    qCDebug(lcListModel) << "Resetting to " << document;
    beginResetModel();
    m_idMap.clear();
    m_data.clear();

    Q_ASSERT(document.isObject());
    if (!document.isObject()) {
        qCWarning(lcListModel) << "Got a document not an object: " << document;
        endResetModel();
        return;
    }

    QJsonObject dataObject = document.object();
    QJsonObject::const_iterator datait;
    if ((datait = dataObject.constFind("data")) == dataObject.constEnd()) {
        qCDebug(lcListModel) << "No data object found";
        endResetModel();
        return; // no rows
    }

    Q_ASSERT(datait.value().isObject());
    QJsonObject rowMap = datait.value().toObject();

    if (rowMap.size() == 0) {
        qCDebug(lcListModel) << "Empty data object";
        endResetModel();
        return; // no rows
    }

    for (QJsonObject::const_iterator rowit = rowMap.constBegin(); rowit != rowMap.constEnd(); rowit++) {
        Q_ASSERT(rowit.value().isObject());
        QJsonObject row = rowit.value().toObject();
        QUuid uuid = rowit.key().toUtf8();
        doSet(uuid, row);
        
    }

    endResetModel();
}

void QBackendJsonListModel::doSet(const QUuid& uuid, const QJsonObject& object, bool shouldEmit)
{
    QMap<QByteArray, QVariant> objectData;

    for (QJsonObject::const_iterator propit = object.constBegin(); propit != object.constEnd(); propit++) {
        objectData[propit.key().toUtf8()] = propit.value().toVariant();
    }

    int rowIdx = -1;
    if (shouldEmit) {
        Q_ASSERT(!m_idMap.contains(uuid));
        rowIdx = m_idMap.indexOf(uuid);
    }

    if (shouldEmit) {
        if (rowIdx == -1) {
            beginInsertRows(QModelIndex(), m_idMap.size(), m_idMap.size());
        }
    }

    if (rowIdx == -1) {
        m_idMap.append(uuid);
        m_data.append(objectData);
    } else {
        m_data[rowIdx] = objectData;
    }

    if (shouldEmit) {
        if (rowIdx == -1) {
            endInsertRows();
        } else {
            // ### roles
            dataChanged(index(rowIdx), index(rowIdx));
        }
    }
}

void QBackendJsonListModel::doRemove(const QUuid& uuid)
{
    int rowIdx = m_idMap.indexOf(uuid);
    Q_ASSERT(rowIdx != -1);

    // ### not performant with a lot of removes
    qCWarning(lcListModel) << "Removing " << uuid << " row ID " << rowIdx;
    beginRemoveRows(QModelIndex(), rowIdx, rowIdx);
    m_idMap.remove(rowIdx);
    endRemoveRows();
}

void QBackendJsonListModelProxy::methodInvoked(const QByteArray& method, const QJsonDocument& document)
{
    if (method == "set") {
        // ### handle arrays, not just objects
        if (!document.isObject()) {
            // uh.. ok
            qCWarning(lcListModel) << "set without a valid object" << document;
            return;
        }

        QJsonObject object = document.object();
        QUuid uuid = object.value("UUID").toString().toUtf8();
        QJsonObject data = object.value("data").toObject();

        qCDebug(lcListModel) << "Updating " << uuid << " to data " << data;

        m_model->doSet(uuid, data, true);
    } else if (method == "remove") {
        qCWarning(lcListModel) << "remove";
        // ### handle arrays, not just objects
        if (!document.isObject()) {
            // uh.. ok
            qCWarning(lcListModel) << "set without a valid object" << document;
            return;
        }

        QJsonObject object = document.object();
        QUuid uuid = object.value("UUID").toString().toUtf8();
        m_model->doRemove(uuid);
    }
}

// ### error on componentComplete if not set
void QBackendJsonListModel::setIdentifier(const QByteArray& id)
{
    if (m_identifier == id) {
        return;
    }

    m_identifier = id;
    subscribeIfReady();
}

void QBackendJsonListModel::subscribeIfReady()
{
    if (!m_connection || m_identifier.isEmpty()) {
        return;
    }

    Q_ASSERT(!m_proxy);

    beginResetModel();
    m_proxy = new QBackendJsonListModelProxy(this);
    m_connection->subscribe(m_identifier, m_proxy);

    m_roleNames.clear();
    m_idMap.clear();

    // ### should validate that the backend knows about these roles
    for (const QString& role : m_flatRoleNames) {
        m_roleNames[Qt::UserRole + m_roleNames.count()] = role.toUtf8();
    }

    m_roleNames[Qt::UserRole + m_roleNames.count()] = "_uuid";

    qCWarning(lcListModel) << "Set model " << m_proxy << " for identifier " << m_identifier << m_roleNames << " rows " << m_idMap.count();
    endResetModel();
}

void QBackendJsonListModel::invokeMethod(const QString& method, const QJSValue& data)
{
    QJSEngine *engine = qmlEngine(this);
    QJSValue global = engine->globalObject();
    QJSValue json = global.property("JSON");
    QJSValue stringify = json.property("stringify");
    QJSValue jsonData = stringify.call(QList<QJSValue>() << data);
    m_connection->invokeMethod(m_identifier, method, jsonData.toString().toUtf8());
}

QHash<int, QByteArray> QBackendJsonListModel::roleNames() const
{
    return m_roleNames;
}

int QBackendJsonListModel::rowCount(const QModelIndex&) const
{
    return m_idMap.size();
}

QVariant QBackendJsonListModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::UserRole + m_roleNames.count() - 1) {
        // uuid request
        return m_idMap.at(index.row()).toString();
    }

    return m_data[index.row()][m_flatRoleNames.at(role - Qt::UserRole).toUtf8()];
}

