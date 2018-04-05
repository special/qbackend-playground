#include <QDebug>

#include "qbackendmodel.h"
#include "qbackendprocess.h"

QBackendModel::QBackendModel(QBackendProcess* connection, const QString& identifier)
    : m_identifier(identifier)
    , m_connection(connection)
{
}

QString QBackendModel::identifier() const
{
    return m_identifier;
}

QBackendModel::QBackendRowData QBackendModel::data(const QUuid& uuid)
{
    return m_data[uuid];
}

QVector<QUuid> QBackendModel::keys()
{
    // yeah... yeah.
    return m_data.keys().toVector();
}

void QBackendModel::appendFromProcess(const QVector<QUuid>& uuids, const QVector<QBackendRowData>& datas)
{
    Q_ASSERT(uuids.length() == datas.length());

    emit aboutToAdd(uuids, datas);
    for (int i = 0; i < uuids.length(); ++i) {
        qDebug() << "Appending " << uuids.at(i) << datas.at(i);
        m_data[uuids.at(i)] = datas.at(i);
    }
    emit added(uuids, datas);
}

void QBackendModel::updateFromProcess(const QVector<QUuid>& uuids, const QVector<QBackendRowData>& datas)
{
    Q_ASSERT(uuids.length() == datas.length());

    // ### do we need the old data? inefficient...
    QVector<QBackendRowData> oldDatas;
    oldDatas.reserve(datas.size());

    for (const QUuid& uuid : uuids) {
        oldDatas.append(m_data[uuid]);
    }

    emit aboutToUpdate(uuids, oldDatas, datas);
    for (int i = 0; i < uuids.length(); ++i) {
        qDebug() << "Updating " << uuids.at(i) << datas.at(i);
        m_data[uuids.at(i)] = datas.at(i);
    }
    emit updated(uuids, oldDatas, datas);
}

void QBackendModel::removeFromProcess(const QVector<QUuid>& uuids)
{
    emit aboutToRemove(uuids);
    for (int i = 0; i < uuids.length(); ++i) {
        qDebug() << "Removing " << uuids.at(i);
        m_data.remove(uuids.at(i));
    }
    emit removed(uuids);
}

void QBackendModel::invokeMethod(const QString& method, const QByteArray& jsonData)
{
    m_connection->invokeMethod(m_identifier, method, jsonData);
}

void QBackendModel::invokeMethodOnObject(const QUuid& uuid, const QString& method, const QByteArray& jsonData)
{
    m_connection->invokeMethodOnObject(m_identifier, uuid, method, jsonData);
}

void QBackendModel::write(const QByteArray& data)
{
    qWarning() << m_connection;
    m_connection->write(data);
}

