#include <iostream>
#include <QDebug>
#include <QQmlEngine>

#include "qbackendlistmodel.h"
#include "qbackendrepository.h"
#include "qbackendmodel.h"

QBackendListModel::QBackendListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QString QBackendListModel::identifier() const
{
    return m_identifier;
}

void QBackendListModel::onAboutToUpdate(const QVector<QUuid>& uuids, const QVector<QVector<QVariant>>& oldDatas, const QVector<QVector<QVariant>>& newDatas)
{
}

void QBackendListModel::onUpdated(const QVector<QUuid>& uuids, const QVector<QVector<QVariant>>& oldDatas, const QVector<QVector<QVariant>>& newDatas)
{
    // ### coalesce updates where possible
    for (const QUuid& uuid : uuids) {
        qWarning() << "Updating " << uuid;
        int rowIdx = m_idMap.indexOf(uuid);
        Q_ASSERT(rowIdx != -1);
        emit dataChanged(index(rowIdx, 0), index(rowIdx, 0));
    }
}

void QBackendListModel::onAboutToAdd(const QVector<QUuid>& uuids, const QVector<QVector<QVariant>>& datas)
{

}

void QBackendListModel::onAdded(const QVector<QUuid>& uuids, const QVector<QVector<QVariant>>& datas)
{
    beginInsertRows(QModelIndex(), m_idMap.size(), m_idMap.size() + uuids.length() - 1);
    for (const QUuid& uuid : uuids) {
        qWarning() << "Appending " << uuid;
        Q_ASSERT(!m_idMap.contains(uuid));
        m_idMap.append(uuid);
    }
    endInsertRows();
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

void QBackendListModel::setIdentifier(const QString& id)
{
    if (m_model) {
        disconnect(m_model, &QBackendModel::aboutToUpdate, this, &QBackendListModel::onAboutToUpdate);
        disconnect(m_model, &QBackendModel::updated, this, &QBackendListModel::onUpdated);
        disconnect(m_model, &QBackendModel::aboutToAdd, this, &QBackendListModel::onAboutToAdd);
        disconnect(m_model, &QBackendModel::added, this, &QBackendListModel::onAdded);
        disconnect(m_model, &QBackendModel::aboutToRemove, this, &QBackendListModel::onAboutToRemove);
        disconnect(m_model, &QBackendModel::removed, this, &QBackendListModel::onRemoved);
    }

    beginResetModel();
    m_identifier = id;
    m_model = QBackendRepository::model(id);

    m_roleNames.clear();
    m_idMap.clear();

    if (m_model) {
        for (const QByteArray& role : m_model->roleNames()) {
            m_roleNames[Qt::UserRole + m_roleNames.count()] = role;
        }
        for (const QUuid& uuid : m_model->keys()) {
            m_idMap.append(uuid);
        }
    }

    m_roleNames[Qt::UserRole + m_roleNames.count()] = "_uuid";

    qWarning() << "Set model " << m_model << " for identifier " << id << m_roleNames << " rows " << m_idMap.count();
    endResetModel();

    if (m_model) {
        connect(m_model, &QBackendModel::aboutToUpdate, this, &QBackendListModel::onAboutToUpdate);
        connect(m_model, &QBackendModel::updated, this, &QBackendListModel::onUpdated);
        connect(m_model, &QBackendModel::aboutToAdd, this, &QBackendListModel::onAboutToAdd);
        connect(m_model, &QBackendModel::added, this, &QBackendListModel::onAdded);
        connect(m_model, &QBackendModel::aboutToRemove, this, &QBackendListModel::onAboutToRemove);
        connect(m_model, &QBackendModel::removed, this, &QBackendListModel::onRemoved);
    }
}

void QBackendListModel::invokeMethod(const QString& method, const QJSValue& data)
{
    QJSEngine *engine = qmlEngine(this);
    QJSValue global = engine->globalObject();
    QJSValue json = global.property("JSON");
    QJSValue stringify = json.property("stringify");
    QJSValue jsonData = stringify.call(QList<QJSValue>() << data);
    m_model->invokeMethod(method, jsonData.toString().toUtf8());
}

void QBackendListModel::invokeMethodOnRow(int index, const QString& method, const QJSValue& data)
{
    QJSEngine *engine = qmlEngine(this);
    QJSValue global = engine->globalObject();
    QJSValue json = global.property("JSON");
    QJSValue stringify = json.property("stringify");
    QJSValue jsonData = stringify.call(QList<QJSValue>() << data);
    m_model->invokeMethodOnObject(m_idMap.at(index), method, jsonData.toString().toUtf8());
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
        return m_idMap.at(index.row());
    }

    return m_model->data(m_idMap.at(index.row())).at(role - Qt::UserRole);
}

