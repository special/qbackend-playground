#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QProcess>

class QBackendObject;

// Used to handle requests from the remote peer.
// Create an instance, and associate it with a given name that will be
// registered by the remote peer to handle method invocations.
class QBackendRemoteObject : public QObject
{
public:
    // Called when an object has been associated with the subscribed identifier
    virtual void objectFound(const QJsonDocument& document) = 0;

    // Called when a method is invoked on this object
    virtual void methodInvoked(const QByteArray& method, const QJsonDocument& document) = 0;
};

// A backend connection is the class that talks to a remote peer and does things
// with it.

class QBackendAbstractConnection : public QObject
{
    Q_OBJECT
public:
    QBackendAbstractConnection(QObject *parent = 0);

public:
    virtual QBackendObject *rootObject() const = 0;
    virtual QBackendObject *object(const QByteArray &identifier) = 0;
    virtual void subscribe(const QByteArray& identifier, QBackendRemoteObject* object) = 0;
    virtual void unsubscribe(const QByteArray& identifier, QBackendRemoteObject* object) = 0;
    virtual void invokeMethod(const QByteArray& identifier, const QString& method, const QByteArray& jsonData) = 0;
};


