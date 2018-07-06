#pragma once

#include <QObject>
#include "qbackendabstractconnection.h"

class BackendObjectPrivate;

class QBackendObject : public QObject
{
public:
    QBackendObject(QBackendAbstractConnection *connection, QByteArray identifier, const QJsonObject &type, QObject *parent = nullptr);
    virtual ~QBackendObject();

    // Used by QBackendConnection for "root" object data, which follows
    // an unusual path
    void resetData(const QJsonObject &data);

    static QMetaObject staticMetaObject;
    virtual const QMetaObject *metaObject() const override;
    virtual int qt_metacall(QMetaObject::Call c, int id, void **argv) override;

private:
    BackendObjectPrivate *d;
    QMetaObject *m_metaObject = nullptr;
};

Q_DECLARE_METATYPE(QBackendObject*)
