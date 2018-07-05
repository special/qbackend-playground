#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QProcess>

class QBackendObject;
class QQmlEngine;

// Used to handle requests from the remote peer.
// Create an instance, and associate it with a given name that will be
// registered by the remote peer to handle method invocations.
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

// A backend connection is the class that talks to a remote peer and does things
// with it.

class QBackendAbstractConnection : public QObject
{
    Q_OBJECT
public:
    QBackendAbstractConnection(QObject *parent = 0);

public:
    virtual QQmlEngine *qmlEngine() const = 0;
    virtual QBackendObject *rootObject() = 0;
    virtual QObject *object(const QByteArray &identifier) const = 0;
    virtual QObject *ensureObject(const QJsonObject &object) = 0;

    virtual void addObjectProxy(const QByteArray& identifier, QBackendRemoteObject* object) = 0;
    virtual void removeObject(const QByteArray& identifier) = 0;
    virtual void resetObjectData(const QByteArray& identifier, bool synchronous = false) = 0;
    virtual void invokeMethod(const QByteArray& identifier, const QString& method, const QJsonArray& params) = 0;
};


