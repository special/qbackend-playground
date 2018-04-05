#pragma once

#include <QObject>
#include <QVector>
#include <QSet>
#include <QVariant>
#include <QUuid>

class QBackendProcess;

// Model instances are owned by the repository. They contain objects identified
// by a UUID. UI models then map this into something more tangible (list or tree
// models) as needed.
//
// Note that QBackendModel is a representation of a remote resource. It
// functions as a cache. Any actions are _not_ instantaneous, to avoid becoming
// out of sync with the remote resource.
//
// This is not a permanent limitation though: with an 'undo' stack, we could
// consider offline action.

class QBackendModel : public QObject
{
    Q_OBJECT
public:
    QString identifier() const;

    // What data is in this model?
    QVector<QByteArray> roleNames() const;

    // Fetch the data for a given UUID.
    // The data is stored in the order of 'roleNames'.
    QVector<QVariant> data(const QUuid& uuid);

    // What data is in this model?
    QVector<QUuid> keys();

    void invokeMethod(const QString& method, const QByteArray& jsonData);
    void invokeMethodOnObject(const QUuid& uuid, const QString& method, const QByteArray& jsonData);
    void write(const QByteArray& data);

signals:
    void aboutToUpdate(const QVector<QUuid>& uuids, const QVector<QVector<QVariant>>& oldDatas, const QVector<QVector<QVariant>>& newDatas);
    void updated(const QVector<QUuid>& uuids, const QVector<QVector<QVariant>>& oldDatas, const QVector<QVector<QVariant>>& newDatas);
    void aboutToAdd(const QVector<QUuid>& uuids, const QVector<QVector<QVariant>>& datas);
    void added(const QVector<QUuid>& uuids, const QVector<QVector<QVariant>>& datas);
    void aboutToRemove(const QVector<QUuid>& uuids);
    void removed(const QVector<QUuid>& uuids);

private:
    QString m_identifier;

    QBackendProcess *m_connection = nullptr; // where do we come from (to persist changes)
    // ### should be the abstract backend connection eventually, not a process

    // connection API
    void appendFromProcess(const QVector<QUuid>& uuids, const QVector<QVector<QVariant>>& datas);
    void removeFromProcess(const QVector<QUuid>& uuids);
    void updateFromProcess(const QVector<QUuid>& uuids, const QVector<QVector<QVariant>>& datas);
    // end connection API

    QVector<QByteArray> m_roleNames;
    QHash<QUuid, QVector<QVariant>> m_data;

    friend class QBackendProcess;
    friend class QBackendRepository;
    QBackendModel(QBackendProcess* connection, const QString &identifier, const QVector<QByteArray>& roleNames);
};

