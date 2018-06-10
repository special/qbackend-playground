#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QIODevice>
#include <QUrl>
#include <QPointer>

#include "qbackendabstractconnection.h"

class QBackendObject;

class QBackendConnection : public QBackendAbstractConnection
{
    Q_OBJECT
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QBackendObject* root READ rootObject NOTIFY rootObjectChanged)

public:
    QBackendConnection(QObject *parent = 0);

    QUrl url() const;
    void setUrl(const QUrl& url);

    QBackendObject *rootObject() const;

    Q_INVOKABLE QBackendObject *object(const QByteArray &identifier);

    void invokeMethod(const QByteArray& identifier, const QString& method, const QByteArray& jsonData) override;
    void subscribe(const QByteArray& identifier, QBackendRemoteObject* object) override;
    void unsubscribe(const QByteArray& identifier, QBackendRemoteObject* object) override;

signals:
    void urlChanged();
    void rootObjectChanged();

protected:
    void setBackendIo(QIODevice *read, QIODevice *write);

private slots:
    void handleDataReady();

private:
    QUrl m_url;
    QIODevice *m_readIo = nullptr;
    QIODevice *m_writeIo = nullptr;
    QList<QByteArray> m_pendingData;

    void handleMessage(const QByteArray &message);
    void write(const QJsonObject &message);

    QMultiHash<QByteArray, QBackendRemoteObject*> m_subscribedObjects;
    QHash<QByteArray, QPointer<QBackendObject>> m_objects;
    QBackendObject *m_rootObject = nullptr;
};

