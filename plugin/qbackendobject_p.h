#pragma once

#include <QObject>
#include <QJsonObject>
#include <QMetaObject>
#include <QJSValue>

#include "qbackendabstractconnection.h"

class BackendObjectPrivate : public QBackendRemoteObject
{
    Q_OBJECT

public:
    QObject *m_object = nullptr;

    QBackendAbstractConnection *m_connection = nullptr;
    QByteArray m_identifier;

    QJsonObject m_dataObject;
    bool m_dataReady = false;

    BackendObjectPrivate(QObject *object, QBackendAbstractConnection *connection, const QByteArray &identifier);
    virtual ~BackendObjectPrivate();

    QObject *object() const override { return m_object; }
    void objectFound(const QJsonObject& object) override;
    void methodInvoked(const QString& method, const QJsonArray& params) override;
    void resetData(const QJsonObject &data);

    int metacall(QMetaObject::Call c, int id, void **argv);

    QMetaObject *metaObjectFromType(const QJsonObject &type, const QMetaObject *superClass = nullptr);
    std::pair<QString,QString> qtTypesFromType(const QString &type);
    void *jsonValueToMetaArgs(QMetaType::Type type, const QJsonValue &value, void *p = nullptr);
    QJSValue jsonValueToJSValue(QJSEngine *engine, const QJsonValue &value);
};
