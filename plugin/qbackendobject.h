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
    Q_PROPERTY(QByteArray identifier READ identifier CONSTANT)
    Q_PROPERTY(QBackendAbstractConnection* connection READ connection CONSTANT)
    Q_PROPERTY(QObject* data READ data NOTIFY dataChanged)
public:
    QBackendObject(QBackendAbstractConnection* connection, QByteArray identifier, QObject *parent = nullptr);

    QByteArray identifier() const;
    QBackendAbstractConnection* connection() const;
    QObject* data() const;

    // ### not public
    void doReset(const QJsonObject& object);

    Q_INVOKABLE void invokeMethod(const QByteArray& method, const QJSValue& data);

signals:
    void dataChanged();

private:
    QVariant readProperty(const QMetaProperty& property);

    QByteArray m_identifier;
    QBackendAbstractConnection *m_connection = nullptr;
    QBackendObjectProxy *m_proxy = nullptr;
    QHash<const char *, QVariant> changedProperties;
    QObject *m_dataObject = nullptr;
};


