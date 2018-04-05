#pragma once

#include <QAbstractListModel>
#include <QJSValue>
#include <QUuid>

class QBackendModel;

class QBackendListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString identifier READ identifier WRITE setIdentifier NOTIFY identifierChanged)
public:
    QBackendListModel(QObject *parent = 0);

    QString identifier() const;
    void setIdentifier(const QString& identifier);

    Q_INVOKABLE void invokeMethod(const QString& method, const QJSValue& data = QJSValue());
    Q_INVOKABLE void invokeMethodOnRow(int index, const QString& method, const QJSValue& data = QJSValue());

signals:
    void identifierChanged();

protected:
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex&) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private slots:
    void onAboutToUpdate(const QVector<QUuid>& uuid, const QVector<QVector<QVariant>>& oldData, const QVector<QVector<QVariant>>& newData);
    void onUpdated(const QVector<QUuid>& uuid, const QVector<QVector<QVariant>>& oldData, const QVector<QVector<QVariant>>& newData);
    void onAboutToAdd(const QVector<QUuid>& uuid, const QVector<QVector<QVariant>>& data);
    void onAdded(const QVector<QUuid>& uuid, const QVector<QVector<QVariant>>& data);
    void onAboutToRemove(const QVector<QUuid>& uuid);
    void onRemoved(const QVector<QUuid>& uuid);

private:
    QString m_identifier;
    QHash<int, QByteArray> m_roleNames;
    QVector<QUuid> m_idMap;
    QBackendModel *m_model = nullptr;
};

