#include <iostream>
#include <QDebug>
#include <QQmlEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "qbackendabstractconnection.h"
#include "qbackendlistmodel.h"

QBackendListModel::QBackendListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QBackendAbstractConnection* QBackendListModel::connection() const
{
    return m_connection;
}

// ### error on componentComplete if not set
void QBackendListModel::setConnection(QBackendAbstractConnection* connection)
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

QStringList QBackendListModel::roles() const
{
    return m_flatRoleNames;
}

// ### error on componentComplete if not set
void QBackendListModel::setRoles(const QStringList &roleNames)
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

QByteArray QBackendListModel::identifier() const
{
    return m_identifier;
}

class QBackendListModelProxy : public QBackendRemoteObject
{
public:
    QBackendListModelProxy(QBackendListModel* model);
    void objectFound(const QJsonDocument& document) override;
    void methodInvoked(const QByteArray& method, const QJsonDocument& document) override;

private:
    QBackendListModel *m_model = nullptr;
};

QBackendListModelProxy::QBackendListModelProxy(QBackendListModel* model)
    : m_model(model)
{
}

void QBackendListModelProxy::objectFound(const QJsonDocument& document)
{
#if 0
    Q_ASSERT(doc.isObject());
    QJsonObject obj = doc.object();

    QMap<QString, QVariant> data;

    for (const QString& key : obj.keys()) {
        data[key.toUtf8()] = obj.value(key).toVariant();
    }
#endif
    m_model->doReset(document);
}

void QBackendListModel::doReset(const QJsonDocument& document)
{
    qDebug() << "Resetting to " << document;
    beginResetModel();
    m_idMap.clear();
    m_data.clear();

    Q_ASSERT(document.isObject());
    if (!document.isObject()) {
        qWarning() << "Got a document not an object: " << document;
        endResetModel();
        return;
    }

    QJsonObject dataObject = document.object();
    QJsonObject::const_iterator datait;
    if ((datait = dataObject.constFind("data")) == dataObject.constEnd()) {
        qDebug() << "No data object found";
        endResetModel();
        return; // no rows
    }

    Q_ASSERT(datait.value().isArray());
    QJsonArray dataArray = datait.value().toArray();

    if (dataArray.size() == 0) {
        qDebug() << "Empty data object";
        endResetModel();
        return; // no rows
    }

    for (int i = 0; i < dataArray.size(); ++i) {
        if (!dataArray.at(i).isObject()) {
            // uh.. ok
            continue;
        }

        QJsonObject row = dataArray.at(i).toObject();
        doAppend(row);
    }

    endResetModel();
}

void QBackendListModel::doAppend(const QJsonObject& object, bool shouldEmit)
{
    QMap<QByteArray, QVariant> objectData;
    bool hasUuid = false;
    QUuid uuid;

    for (QJsonObject::const_iterator propit = object.constBegin(); propit != object.constEnd(); propit++) {
        if (propit.key() == "UUID") {
            hasUuid = true;
            uuid = propit.value().toString().toUtf8();
        } else {
            objectData[propit.key().toUtf8()] = propit.value().toVariant();
        }
    }

    if (hasUuid) {
        Q_ASSERT(!m_idMap.contains(uuid));

        if (shouldEmit) {
            beginInsertRows(QModelIndex(), m_idMap.size(), m_idMap.size());
        }
        m_idMap.append(uuid);
        m_data.append(objectData);
        if (shouldEmit) {
            endInsertRows();
        }
    }
}

void QBackendListModelProxy::methodInvoked(const QByteArray& method, const QJsonDocument& document)
{
    if (method == "append") {
        // ### handle arrays, not just objects
        if (!document.isObject()) {
            // uh.. ok
            qWarning() << "append without a valid object" << document;
            return;
        }

        qDebug() << "append" << document;
        QJsonObject row = document.object();
        m_model->doAppend(row, true);
    }
    if (method == "update") {
        qWarning() << "update";
    }
    if (method == "remove") {
        qWarning() << "remove";
    }

#if 0
    for (const QUuid& uuid : m_model->keys()) {
        m_idMap.append(uuid);
    }
void QBackendListModel::onAboutToUpdate(const QVector<QUuid>& uuids, const QVector<QBackendModel::QBackendRowData>& oldDatas, const QVector<QBackendModel::QBackendRowData>& newDatas)
{
}

void QBackendListModel::onUpdated(const QVector<QUuid>& uuids, const QVector<QBackendModel::QBackendRowData>& oldDatas, const QVector<QBackendModel::QBackendRowData>& newDatas)
{
    // ### coalesce updates where possible
    for (const QUuid& uuid : uuids) {
        qWarning() << "Updating " << uuid;
        int rowIdx = m_idMap.indexOf(uuid);
        Q_ASSERT(rowIdx != -1);
        emit dataChanged(index(rowIdx, 0), index(rowIdx, 0));
    }
}

void QBackendListModel::onAboutToAdd(const QVector<QUuid>& uuids, const QVector<QBackendModel::QBackendRowData>& datas)
{

}

void QBackendListModel::onAdded(const QVector<QUuid>& uuids, const QVector<QBackendModel::QBackendRowData>& datas)
{
}

void QBackendListModel::onAboutToRemove(const QVector<QUuid>& uuids)
{

}

void QBackendListModel::onRemoved(const QVector<QUuid>& uuids)
{
    for (const QUuid& uuid : uuids) {
        int rowIdx = m_idMap.indexOf(uuid);
        Q_ASSERT(rowIdx != -1);

        // ### not performant with a lot of removes
        qWarning() << "Removing " << uuid << " row ID " << rowIdx;
        beginRemoveRows(QModelIndex(), rowIdx, rowIdx);
        m_idMap.remove(rowIdx);
        endRemoveRows();
    }
}
#endif
}

// ### error on componentComplete if not set
void QBackendListModel::setIdentifier(const QByteArray& id)
{
    if (m_identifier == id) {
        return;
    }

    m_identifier = id;
    subscribeIfReady();
}

void QBackendListModel::subscribeIfReady()
{
    if (!m_connection || m_identifier.isEmpty()) {
        return;
    }

    Q_ASSERT(!m_proxy);

    beginResetModel();
    m_proxy = new QBackendListModelProxy(this);
    m_connection->subscribe(m_identifier, m_proxy);

    m_roleNames.clear();
    m_idMap.clear();

    // ### should validate that the backend knows about these roles
    for (const QString& role : m_flatRoleNames) {
        m_roleNames[Qt::UserRole + m_roleNames.count()] = role.toUtf8();
    }

    m_roleNames[Qt::UserRole + m_roleNames.count()] = "_uuid";

    qWarning() << "Set model " << m_proxy << " for identifier " << m_identifier << m_roleNames << " rows " << m_idMap.count();
    endResetModel();
}

void QBackendListModel::invokeMethod(const QString& method, const QJSValue& data)
{
    QJSEngine *engine = qmlEngine(this);
    QJSValue global = engine->globalObject();
    QJSValue json = global.property("JSON");
    QJSValue stringify = json.property("stringify");
    QJSValue jsonData = stringify.call(QList<QJSValue>() << data);
    m_connection->invokeMethod(m_identifier, method, jsonData.toString().toUtf8());
}

QHash<int, QByteArray> QBackendListModel::roleNames() const
{
    return m_roleNames;
}

int QBackendListModel::rowCount(const QModelIndex&) const
{
    return m_idMap.size();
}

QVariant QBackendListModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::UserRole + m_roleNames.count() - 1) {
        // uuid request
        return m_idMap.at(index.row()).toString();
    }

    return m_data[index.row()][m_flatRoleNames.at(role - Qt::UserRole).toUtf8()];
}

