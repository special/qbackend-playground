#include <QDebug>
#include <QMetaObject>
#include <QMetaProperty>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJSValue>
#include <QLoggingCategory>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QtCore/private/qmetaobjectbuilder_p.h>
#include "qbackendobject.h"
#include "qbackendobject_p.h"

Q_LOGGING_CATEGORY(lcObject, "backend.object")

QBackendObject::QBackendObject(QBackendAbstractConnection *connection, QByteArray identifier, const QJsonObject &type, QObject *parent)
    : QObject(parent)
    , d(new BackendObjectPrivate(this, connection, identifier))
    , m_metaObject(d->metaObjectFromType(type))
{
}

QBackendObject::~QBackendObject()
{
    d->deleteLater();
    // XXX clean up proxy, subscription, etc
    free(m_metaObject);
}

const QMetaObject *QBackendObject::metaObject() const
{
    Q_ASSERT(m_metaObject);
    return m_metaObject;
}

int QBackendObject::qt_metacall(QMetaObject::Call c, int id, void **argv)
{
    id = QObject::qt_metacall(c, id, argv);
    if (id < 0)
        return id;
    return d->metacall(c, id, argv);
}

void QBackendObject::resetData(const QJsonObject &data)
{
    d->resetData(data);
}

BackendObjectPrivate::BackendObjectPrivate(QObject *object, QBackendAbstractConnection *connection, const QByteArray &identifier)
    : m_object(object)
    , m_connection(connection)
    , m_identifier(identifier)
{
}

BackendObjectPrivate::~BackendObjectPrivate()
{
    // XXX unsubscribe or whatever else is necessary here
}

void BackendObjectPrivate::objectFound(const QJsonObject &object)
{
    resetData(object);
}

void BackendObjectPrivate::methodInvoked(const QString &name, const QJsonArray &params)
{
    // Technically, this should find the signal by its full signature, to enable overloads.
    // Since we're mirroring a Go object, overloaded names don't really make sense, so we can
    // cheat and disallow them.
    const QMetaObject *metaObject = m_object->metaObject();
    for (int i = metaObject->methodOffset(); i < metaObject->methodCount(); i++) {
        QMetaMethod method = metaObject->method(i);
        if (method.methodType() != QMetaMethod::Signal || method.name() != name)
            continue;

        if (method.parameterCount() != params.count()) {
            qCWarning(lcObject) << "Signal" << method.name() << "emitted with incorrect parameters; expected" << method.methodSignature() << "got parameters" << params;
            break;
        }

        // Marshal arguments for the signal. [0] is for return value, which is void for signals.
        QVector<void*> argv(method.parameterCount()+1);
        for (int j = 0; j < method.parameterCount(); j++) {
            QMetaType::Type paramType = static_cast<QMetaType::Type>(method.parameterType(j));
            argv[j+1] = jsonValueToMetaArgs(paramType, params[j], nullptr);
        }

        qCDebug(lcObject) << "Emitting signal" << name << "with args" << params;
        QMetaObject::activate(m_object, i, argv.data());

        // Free parameters in argv
        for (int j = 0; j < method.parameterCount(); j++)
            QMetaType::destroy(method.parameterType(j), argv[j+1]);

        break;
    }
}

void BackendObjectPrivate::resetData(const QJsonObject& object)
{
    qCDebug(lcObject) << "Resetting " << m_identifier << " to " << object;
    m_dataObject = object;
    m_dataReady = true;

    // XXX Do something smarter than signaling for every property
    // XXX This is also wrong: any properties in the old m_dataObject that
    // aren't in object have also changed.
    const QMetaObject *metaObject = m_object->metaObject();
    for (auto it = m_dataObject.constBegin(); it != m_dataObject.constEnd(); it++) {
        int index = metaObject->indexOfProperty(it.key().toUtf8());
        if (index < 0)
            continue;
        QMetaProperty property = metaObject->property(index);
        int notifyIndex = property.notifySignalIndex();
        if (notifyIndex >= 0)
            QMetaObject::activate(m_object, notifyIndex, nullptr);
    }
}

// Called by the front m_object's qt_metacall to handle backend calls
int BackendObjectPrivate::metacall(QMetaObject::Call c, int id, void **argv)
{
    const QMetaObject *metaObject = m_object->metaObject();

    if (c == QMetaObject::ReadProperty) {
        int count = metaObject->propertyCount() - metaObject->propertyOffset();
        QMetaProperty property = metaObject->property(id + metaObject->propertyOffset());

        if (!m_dataReady) {
            // XXX This ends up in objectFound and sends notify signals, which sounds a little
            // dangerous.. I could imagine it creating fake binding loops. They could be deferred
            // I guess?
            qCDebug(lcObject) << "Blocking to load data for object" << m_identifier << "from read of property" << property.name();
            m_connection->subscribeSync(m_identifier, this);
        }

        jsonValueToMetaArgs(static_cast<QMetaType::Type>(property.userType()), m_dataObject.value(property.name()), argv[0]);

        id -= count;
    } else if (c == QMetaObject::InvokeMetaMethod) {
        int count = metaObject->methodCount() - metaObject->methodOffset();
        QMetaMethod method = metaObject->method(id + metaObject->methodOffset());

        if (method.isValid()) {
            QJsonArray args;
            for (int i = 0; i < method.parameterCount(); i++) {
                switch (method.parameterType(i)) {
                case QMetaType::Bool:
                    args.append(QJsonValue(*reinterpret_cast<bool*>(argv[i+1])));
                    break;
                case QMetaType::Double:
                    args.append(QJsonValue(*reinterpret_cast<double*>(argv[i+1])));
                    break;
                case QMetaType::Int:
                    args.append(QJsonValue(*reinterpret_cast<int*>(argv[i+1])));
                    break;
                case QMetaType::QString:
                    args.append(QJsonValue(*reinterpret_cast<QString*>(argv[i+1])));
                    break;
                case QMetaType::QVariant:
                    args.append(reinterpret_cast<QVariant*>(argv[i+1])->toJsonValue());
                    break;
                default:
                    // XXX
                    break;
                }
            }

            m_connection->invokeMethod(m_identifier, QString::fromUtf8(method.name()), args);
        }

        id -= count;
    }

    return id;
}

// Construct a copy of 'v' (which is type 'type') at 'p', or allocate if 'p' is nullptr
template<typename T> static void *copyMetaArg(QMetaType::Type type, void *p, const T &v)
{
    if (!p)
        p = QMetaType::create(type, reinterpret_cast<const void*>(&v));
    else
        p = QMetaType::construct(type, p, reinterpret_cast<const void*>(&v));
    return p;
}

void *BackendObjectPrivate::jsonValueToMetaArgs(QMetaType::Type type, const QJsonValue &value, void *p)
{
    switch (type) {
    case QMetaType::Bool:
        p = copyMetaArg(type, p, value.toBool());
        break;

    case QMetaType::Double:
        p = copyMetaArg(type, p, value.toDouble());
        break;

    case QMetaType::Int:
        p = copyMetaArg(type, p, value.toInt());
        break;

    case QMetaType::QString:
        p = copyMetaArg(type, p, value.toString());
        break;

    case QMetaType::QVariant:
        p = copyMetaArg(type, p, value.toVariant());
        break;

    case QMetaType::QObjectStar:
        {
            QObject *v = m_connection->ensureObject(value.toObject());
            if (!p)
                p = QMetaType::create(type, reinterpret_cast<void*>(&v));
            else
                *reinterpret_cast<QObject**>(p) = v;
        }
        break;

    default:
        // XXX May be possible to do some QVariant conversion here?
        qCWarning(lcObject) << "Unknown type" << QMetaType::typeName(type) << "in JSON value conversion";
        break;
    }

    return p;
}

/* Type definitions:
 *
 * {
 *   "name": "Person",
 *   "properties": {
 *     "fullName": "string",
 *     "id": { "type": "int", "readonly": true }
 *   },
 *   "methods": {
 *     "greet": [ "string", "bool" ]
 *   },
 *   "signals": {
 *     "died": [ "string", "int" ]
 *   }
 * }
 *
 * valid type strings are: string, int, double, bool, var, object
 * object is a qbackend object; it will contain the object structure.
 * var can hold any JSON type, including JSON objects.
 * var can also hold qbackend objects, which are recognized by a special property.
 */

/* Object structure:
 *
 * {
 *   "_qbackend_": "object",
 *   "identifier": "123",
 *   // This is a full type definition object for types that have not been previously defined
 *   "type": "Person",
 *   "data": {
 *     "fullName": "Abazza Bipedal",
 *     "id": 6
 *   }
 * }
 *
 * These are tagged with _qbackend_ to allow them to be identified as values in data,
 * even if the type is not strict.
 *
 * Unless otherwise noted, "data" is comprehensive and any property not included gets a default value.
 */

// XXX error handling
QMetaObject *BackendObjectPrivate::metaObjectFromType(const QJsonObject &type, const QMetaObject *superClass)
{
    QMetaObjectBuilder b;
    b.setClassName(type.value("name").toString().toUtf8());
    if (superClass)
        b.setSuperClass(superClass);

    qCDebug(lcObject) << "Building metaobject for type:" << type;

    QJsonObject properties = type.value("properties").toObject();
    for (auto it = properties.constBegin(); it != properties.constEnd(); it++) {
        // XXX readonly: true syntax, notifiers, other things
        qCDebug(lcObject) << " -- property:" << it.key() << it.value().toString();
        int notifier = b.addSignal(it.key().toUtf8() + "Changed()").index();
        b.addProperty(it.key().toUtf8(), qtTypesFromType(it.value().toString()).first.toUtf8(), notifier);
    }

    QJsonObject signalsObj = type.value("signals").toObject();
    for (auto it = signalsObj.constBegin(); it != signalsObj.constEnd(); it++) {
        QString signature = it.key() + "(";
        QList<QByteArray> paramNames;
        QJsonArray params = it.value().toArray();
        for (const QJsonValue &p : params) {
            auto pv = p.toString().split(" ");
            signature += qtTypesFromType(pv[0]).first + ",";
            paramNames.append(pv[1].toUtf8());
        }
        if (signature.endsWith(",")) {
            signature.chop(1);
        }
        signature += ")";
        QMetaMethodBuilder method = b.addSignal(signature.toUtf8());
        method.setParameterNames(paramNames);
        qCDebug(lcObject) << " -- signal:" << signature << method.index();
    }

    QJsonObject methods = type.value("methods").toObject();
    for (auto it = methods.constBegin(); it != methods.constEnd(); it++) {
        // XXX lots of things also
        QString name = it.key();
        // XXX can't just go changing this..
        //name = name[0].toLower() + name.mid(1);
        QString signature = name + "(";
        QJsonArray paramTypes = it.value().toArray();
        for (const QJsonValue &type : paramTypes) {
            signature += qtTypesFromType(type.toString()).first + ",";
        }
        if (signature.endsWith(",")) {
            signature.chop(1);
        }
        signature += ")";
        qCDebug(lcObject) << " -- method:" << name << signature;
        b.addMethod(signature.toUtf8());
    }

    return b.toMetaObject();
}

// Qt, QML
std::pair<QString,QString> BackendObjectPrivate::qtTypesFromType(const QString &type)
{
    if (type == "string")
        return {"QString","string"};
    else if (type == "int")
        return {"int","int"};
    else if (type == "double")
        return {"double","double"};
    else if (type == "bool")
        return {"bool","bool"};
    else if (type == "var")
        return {"QVariant","var"};
    else if (type == "object")
        return {"QObject*","var"};
    else
        return {"QVariant","var"};
}

