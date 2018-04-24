#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QIODevice>
#include <QUrl>

#include "qbackendabstractconnection.h"

class QBackendConnection : public QBackendAbstractConnection
{
    Q_OBJECT
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)

public:
    QBackendConnection(QObject *parent = 0);

    QUrl url() const;
    void setUrl(const QUrl& url);

    void invokeMethod(const QByteArray& identifier, const QString& method, const QByteArray& jsonData) override;
    void subscribe(const QByteArray& identifier, QBackendRemoteObject* object) override;

signals:
    void urlChanged();

protected:
    void setBackendIo(QIODevice *read, QIODevice *write);

private slots:
    void handleModelDataReady();

private:
    QUrl m_url;
    QIODevice *m_readIo = nullptr;
    QIODevice *m_writeIo = nullptr;
    QList<QByteArray> m_pendingData;

    QJsonDocument readJsonBlob(int byteCount);
    void write(const QByteArray& data);

    QHash<QByteArray, QBackendRemoteObject*> m_subscribedObjects;
};

