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
public:
    QBackendObject(QBackendAbstractConnection *connection, QByteArray identifier, const QJsonObject &type, QObject *parent = nullptr);
    virtual ~QBackendObject();

    QByteArray identifier() const;
    QBackendAbstractConnection* connection() const;

    // ### not public
    void doReset(const QJsonObject& object);

    Q_INVOKABLE void invokeMethod(const QByteArray& method, const QJSValue& data);

    virtual const QMetaObject *metaObject() const override;
    virtual int qt_metacall(QMetaObject::Call c, int id, void **argv) override;

private:
    QVariant readProperty(const QMetaProperty& property);

    QByteArray m_identifier;
    QBackendAbstractConnection *m_connection = nullptr;
    QBackendObjectProxy *m_proxy = nullptr;
    QMetaObject *m_metaObject = nullptr;
    QJsonObject m_dataObject;
};

Q_DECLARE_METATYPE(QBackendObject*)
