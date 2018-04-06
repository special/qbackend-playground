#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QProcess>

#include "qbackendabstractconnection.h"

class QBackendModel;

// A backend process is one form of RPC. It is not the only form of RPC.
// It populates the repository with properties, models, and so on.

class QBackendProcess : public QBackendAbstractConnection, public QQmlParserStatus
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QStringList args READ args WRITE setArgs NOTIFY argsChanged)
public:
    QBackendProcess(QObject *parent = 0);

    QString name() const;
    void setName(const QString& name);

    QStringList args() const;
    void setArgs(const QStringList& args);

protected:
    void classBegin() override;
    void componentComplete() override;

public:
    void invokeMethod(const QByteArray& identifier, const QString& method, const QByteArray& jsonData) override;
    void subscribe(const QByteArray& identifier, QBackendRemoteObject* object) override;

signals:
    void nameChanged();
    void argsChanged();

private slots:
    void handleModelDataReady();

private:
    QString m_name;
    QStringList m_args;
    bool m_completed = false;
    QProcess m_process;
    QList<QByteArray> m_pendingData;

    QJsonDocument readJsonBlob(int byteCount);
    void write(const QByteArray& data);

    QHash<QByteArray, QBackendRemoteObject*> m_subscribedObjects;
};

