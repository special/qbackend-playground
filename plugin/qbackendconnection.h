#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QIODevice>
#include <QUrl>
#include <QPointer>
#include <QJsonObject>
#include <QQmlEngine>
#include <functional>

#include "qbackendabstractconnection.h"

class QBackendObject;

class QBackendConnection : public QBackendAbstractConnection, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QBackendObject* root READ rootObject NOTIFY ready)

public:
    QBackendConnection(QObject *parent = nullptr);
    QBackendConnection(QQmlEngine *engine);

    // When QBackendConnection is a singleton, qmlEngine/qmlContext may not always work.
    // This will return the explicit engine as well, if one is known.
    QQmlEngine *qmlEngine() const { return m_qmlEngine ? m_qmlEngine : ::qmlEngine(this); };

    QUrl url() const;
    void setUrl(const QUrl& url);

    QBackendObject *rootObject();

    Q_INVOKABLE QObject *object(const QByteArray &identifier) const;
    QObject *ensureObject(const QJsonObject &object);

    void invokeMethod(const QByteArray& identifier, const QString& method, const QJsonArray& params) override;
    void addObjectProxy(const QByteArray& identifier, QBackendRemoteObject* object) override;
    void removeObject(const QByteArray& identifier) override;
    void resetObjectData(const QByteArray& identifier, bool synchronous = false) override;

signals:
    void urlChanged();
    void ready();

protected:
    void setBackendIo(QIODevice *read, QIODevice *write);
    void classBegin() override;
    void componentComplete() override;

private slots:
    void handleDataReady();

private:
    // Try qmlEngine also; this is for singletons or other contexts where engine is explicit
    QQmlEngine *m_qmlEngine = nullptr;

    QUrl m_url;
    QIODevice *m_readIo = nullptr;
    QIODevice *m_writeIo = nullptr;
    QList<QByteArray> m_pendingData;

    bool ensureConnectionConfig();
    bool ensureConnectionReady();

    void handleMessage(const QByteArray &message);
    void write(const QJsonObject &message);

    QJsonObject waitForMessage(std::function<bool(const QJsonObject&)> callback);
    std::function<bool(const QJsonObject&)> m_syncCallback;
    QJsonObject m_syncResult;

    // Hash of identifier -> proxy object for all existing objects
    QHash<QByteArray,QBackendRemoteObject*> m_objects;
    QBackendObject *m_rootObject = nullptr;
};

