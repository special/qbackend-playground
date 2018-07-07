#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QIODevice>
#include <QUrl>
#include <QPointer>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

class QBackendObject;
class QQmlEngine;

class QBackendRemoteObject : public QObject
{
public:
    QBackendRemoteObject(QObject *parent = nullptr) : QObject(parent) { }

    virtual QObject *object() const = 0;

    // Called when an object has been associated with the subscribed identifier
    virtual void objectFound(const QJsonObject& object) = 0;

    // Called when a method is invoked on this object
    virtual void methodInvoked(const QString& method, const QJsonArray& params) = 0;
};

class QBackendConnection : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QBackendObject* root READ rootObject NOTIFY ready)

public:
    QBackendConnection(QObject *parent = nullptr);
    QBackendConnection(QQmlEngine *engine);

    QQmlEngine *qmlEngine() const;
    void setQmlEngine(QQmlEngine *engine);

    QUrl url() const;
    void setUrl(const QUrl& url);

    QBackendObject *rootObject();

    Q_INVOKABLE QObject *object(const QByteArray &identifier) const;
    QObject *ensureObject(const QJsonObject &object);

    void registerTypes(const char *uri);
    void blockReadSignals(bool blocked);

    void invokeMethod(const QByteArray& identifier, const QString& method, const QJsonArray& params);
    void addObjectProxy(const QByteArray& identifier, QBackendRemoteObject* object);
    void removeObject(const QByteArray& identifier);
    void resetObjectData(const QByteArray& identifier, bool synchronous = false);

    void moveToThread(QThread *thread);

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
    int m_version = 0;
    bool m_blockReadSignals = false;

    bool ensureConnectionConfig();
    bool ensureConnectionInit();
    bool ensureRootObject();

    void handleMessage(const QByteArray &message);
    void handleMessage(const QJsonObject &message);
    void handlePendingMessages();
    void write(const QJsonObject &message);

    QJsonObject waitForMessage(std::function<bool(const QJsonObject&)> callback);
    QList<QJsonObject> m_pendingMessages;
    std::function<bool(const QJsonObject&)> m_syncCallback;
    QJsonObject m_syncResult;

    // Hash of identifier -> proxy object for all existing objects
    QHash<QByteArray,QBackendRemoteObject*> m_objects;
    QBackendObject *m_rootObject = nullptr;
    QJsonArray m_creatableTypes;
};

