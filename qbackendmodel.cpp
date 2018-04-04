#include <QDebug>

#include "qbackendmodel.h"
#include "qbackendprocess.h"

QBackendModel::QBackendModel(const QString& identifier, const QVector<QByteArray>& roleNames)
    : m_identifier(identifier)
    , m_roleNames(roleNames)
{

}

QString QBackendModel::identifier() const
{
    return m_identifier;
}

QVector<QByteArray> QBackendModel::roleNames() const
{
    return m_roleNames;
}

QVector<QVariant> QBackendModel::data(const QUuid& uuid)
{
    return m_data[uuid];
}

QUuid QBackendModel::add(const QVector<QVariant>& data)
{
    QUuid id = QUuid::createUuid();
    m_connection->add(id, data);
    return id;
}

void QBackendModel::set(const QUuid& uuid, const QByteArray& role, const QVariant& data)
{
    m_connection->set(uuid, role, data);
}

void QBackendModel::remove(const QUuid& uuid)
{
    m_connection->remove(uuid);
}

QVector<QUuid> QBackendModel::keys()
{
    // yeah... yeah.
    return m_data.keys().toVector();
}

void QBackendModel::appendFromProcess(const QVector<QUuid>& uuids, const QVector<QVector<QVariant>>& datas)
{
    Q_ASSERT(uuids.length() == datas.length());

    emit aboutToAdd(uuids, datas);
    for (int i = 0; i < uuids.length(); ++i) {
        qDebug() << "Appending " << uuids.at(i) << datas.at(i);
        m_data[uuids.at(i)] = datas.at(i);
    }
    emit added(uuids, datas);
}

