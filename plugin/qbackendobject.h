#pragma once

#include <QObject>
#include <QTimerEvent>
#include <QHash>
#include <QVariant>
#include <QJSValue>
#include "qbackendabstractconnection.h"

class QMetaProperty;
class QBackendObjectProxy;

class QBackendObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QByteArray identifier READ identifier WRITE setIdentifier NOTIFY identifierChanged)
    Q_PROPERTY(QBackendAbstractConnection* connection READ connection WRITE setConnection NOTIFY connectionChanged)
    Q_PROPERTY(QObject* data READ data NOTIFY dataChanged)
public:
    QBackendObject(QObject *parent = 0);

    QByteArray identifier() const;
    void setIdentifier(const QByteArray& identifier);

    QBackendAbstractConnection* connection() const;
    void setConnection(QBackendAbstractConnection* connection);

    QObject* data() const;

    // ### not public
    void doReset(const QJsonDocument& document);

    Q_INVOKABLE void invokeMethod(const QByteArray& method, const QJSValue& data);

signals:
    void identifierChanged();
    void connectionChanged();
    void dataChanged();

private:
    QVariant readProperty(const QMetaProperty& property);
    void subscribeIfReady();

    QByteArray m_identifier;
    QBackendAbstractConnection *m_connection = nullptr;
    QBackendObjectProxy *m_proxy = nullptr;
    QHash<const char *, QVariant> changedProperties;
    QObject *m_dataObject = nullptr;
};


