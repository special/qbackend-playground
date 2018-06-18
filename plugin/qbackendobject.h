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

    virtual const QMetaObject *metaObject() const override;
    virtual int qt_metacall(QMetaObject::Call c, int id, void **argv) override;

private:
    friend class QBackendObjectProxy;

    QVariant readProperty(const QMetaProperty& property);

    QByteArray m_identifier;
    QBackendAbstractConnection *m_connection = nullptr;
    QBackendObjectProxy *m_proxy = nullptr;
    QMetaObject *m_metaObject = nullptr;
    QJsonObject m_dataObject;
    bool m_dataReady = false;

    void *jsonValueToMetaArgs(QMetaType::Type type, const QJsonValue &value, void *p = nullptr);
};

Q_DECLARE_METATYPE(QBackendObject*)
