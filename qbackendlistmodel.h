#pragma once

#include <QAbstractListModel>
#include <QJSValue>
#include <QUuid>

#include "qbackendmodel.h"

class QBackendListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString identifier READ identifier WRITE setIdentifier NOTIFY identifierChanged)
    Q_PROPERTY(QStringList roles READ roles WRITE setRoles NOTIFY roleNamesChanged)
    Q_PROPERTY(QBackendProcess* connection READ connection WRITE setConnection NOTIFY connectionChanged)
public:
    QBackendListModel(QObject *parent = 0);

    QString identifier() const;
    void setIdentifier(const QString& identifier);

    QStringList roles() const;
    void setRoles(const QStringList & roleNames);

    QBackendProcess* connection() const;
    void setConnection(QBackendProcess* connection);

    Q_INVOKABLE void invokeMethod(const QString& method, const QJSValue& data = QJSValue());
    Q_INVOKABLE void invokeMethodOnRow(int index, const QString& method, const QJSValue& data = QJSValue());

signals:
    void identifierChanged();
    void roleNamesChanged();
    void connectionChanged();

protected:
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex&) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private slots:
    void onAboutToUpdate(const QVector<QUuid>& uuid, const QVector<QBackendModel::QBackendRowData>& oldData, const QVector<QBackendModel::QBackendRowData>& newData);
    void onUpdated(const QVector<QUuid>& uuid, const QVector<QBackendModel::QBackendRowData>& oldData, const QVector<QBackendModel::QBackendRowData>& newData);
    void onAboutToAdd(const QVector<QUuid>& uuid, const QVector<QBackendModel::QBackendRowData>& data);
    void onAdded(const QVector<QUuid>& uuid, const QVector<QBackendModel::QBackendRowData>& data);
    void onAboutToRemove(const QVector<QUuid>& uuid);
    void onRemoved(const QVector<QUuid>& uuid);

private:
    QString m_identifier;
    QStringList m_flatRoleNames;
    QHash<int, QByteArray> m_roleNames;
    QVector<QUuid> m_idMap;
    QBackendModel *m_model = nullptr;
    QBackendProcess *m_connection = nullptr;
};

