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
#include "qbackendabstractconnection.h"

Q_LOGGING_CATEGORY(lcObject, "backend.object")

class QBackendObjectProxy : public QBackendRemoteObject
{
public:
    QBackendObjectProxy(QBackendObject* model);
    void objectFound(const QJsonObject& object) override;
    void methodInvoked(const QString& method, const QJsonValue& params) override;

private:
    QBackendObject *m_object = nullptr;
};

QMetaObject *metaObjectFromType(const QJsonObject &type);
std::pair<QString,QString> qtTypesFromType(const QString &type);

QBackendObject::QBackendObject(QBackendAbstractConnection *connection, QByteArray identifier, const QJsonObject &type, QObject *parent)
    : QObject(parent)
    , m_identifier(identifier)
    , m_connection(connection)
    , m_metaObject(metaObjectFromType(type))
{
    m_proxy = new QBackendObjectProxy(this);
}

QBackendObject::~QBackendObject()
{
    // XXX clean up proxy, subscription, etc
    free(m_metaObject);
}

/* Identifier is a constant, unique, arbitrary identifier for this object;
 * it can be thought of as equivalent to a pointer, except that identifiers
 * are never reused within a connection. An object's identifier is generated
 * when it is created and cannot change. */
QByteArray QBackendObject::identifier() const
{
    return m_identifier;
}

QBackendAbstractConnection* QBackendObject::connection() const
{
    return m_connection;
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

    if (c == QMetaObject::ReadProperty) {
        if (!m_dataReady) {
            // XXX This ends up in doReset and sends notify signals, which sounds a little
            // dangerous.. I could imagine it creating fake binding loops. They could be deferred
            // I guess?
            m_connection->subscribeSync(m_identifier, m_proxy);
        }

        int count = m_metaObject->propertyCount() - m_metaObject->propertyOffset();
        QMetaProperty property = m_metaObject->property(id + m_metaObject->propertyOffset());

        if (property.isValid()) {
            QJsonValue value = m_dataObject.value(property.name());

            switch (static_cast<QMetaType::Type>(property.type())) {
            case QMetaType::Bool:
                *reinterpret_cast<bool*>(argv[0]) = value.toBool();
                break;
            case QMetaType::Double:
                *reinterpret_cast<double*>(argv[0]) = value.toDouble();
                break;
            case QMetaType::Int:
                *reinterpret_cast<int*>(argv[0]) = value.toInt();
                break;
            case QMetaType::QString:
                *reinterpret_cast<QString*>(argv[0]) = value.toString();
                break;
            case QMetaType::QVariant:
                *reinterpret_cast<QVariant*>(argv[0]) = value.toVariant();
                break;
            default:
                if (property.userType() == QMetaType::type("QBackendObject*")) {
                    *reinterpret_cast<QBackendObject**>(argv[0]) = m_connection->ensureObject(value.toObject());
                } else {
                    // XXX May be possible to do some QVariant conversion here?
                    qCWarning(lcObject) << "Unknown type" << property.typeName() << "in property read of" << property.name();
                }
                break;
            }
        }

        id -= count;
    } else if (c == QMetaObject::InvokeMetaMethod) {
        int count = m_metaObject->methodCount() - m_metaObject->methodOffset();
        QMetaMethod method = m_metaObject->method(id + m_metaObject->methodOffset());

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

QBackendObjectProxy::QBackendObjectProxy(QBackendObject* object)
    : m_object(object)
{

}

void QBackendObjectProxy::objectFound(const QJsonObject& object)
{
    m_object->doReset(object);
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
// XXX error handling
QMetaObject *metaObjectFromType(const QJsonObject &type)
{
    QMetaObjectBuilder b;
    b.setClassName(type.value("name").toString().toUtf8());

    qCDebug(lcObject) << "Building metaobject for type:" << type;

    QJsonObject properties = type.value("properties").toObject();
    for (auto it = properties.constBegin(); it != properties.constEnd(); it++) {
        // XXX readonly: true syntax, notifiers, other things
        qCDebug(lcObject) << " -- property:" << it.key() << it.value().toString();
        int notifier = b.addSignal(it.key().toUtf8() + "Changed()").index();
        b.addProperty(it.key().toUtf8(), qtTypesFromType(it.value().toString()).first.toUtf8(), notifier);
    }

    QJsonObject methods = type.value("methods").toObject();
    for (auto it = methods.constBegin(); it != methods.constEnd(); it++) {
        // XXX lots of things also
        QString name = it.key();
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

    QJsonObject signalsObj = type.value("signals").toObject();
    for (auto it = signalsObj.constBegin(); it != signalsObj.constEnd(); it++) {
        // XXX this is just a copy of the code for methods
        // XXX lots of things also
        QString signature = it.key() + "(";
        QJsonArray paramTypes = it.value().toArray();
        for (const QJsonValue &type : paramTypes) {
            signature += qtTypesFromType(type.toString()).first + ",";
        }
        if (signature.endsWith(",")) {
            signature.chop(1);
        }
        signature += ")";
        qCDebug(lcObject) << " -- signal:" << signature;
        b.addSignal(signature.toUtf8());
    }

    return b.toMetaObject();
}

// Qt, QML
std::pair<QString,QString> qtTypesFromType(const QString &type)
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
        return {"QBackendObject*","BackendObject"};
    else
        return {"QVariant","var"};
}

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

void QBackendObject::doReset(const QJsonObject& object)
{
    qCDebug(lcObject) << "Resetting " << m_identifier << " to " << object;
    m_dataObject = object;
    m_dataReady = true;

    // XXX Do something smarter than signaling for every property
    // XXX This is also wrong: any properties in the old m_dataObject that
    // aren't in object have also changed.
    for (auto it = m_dataObject.constBegin(); it != m_dataObject.constEnd(); it++) {
        int index = metaObject()->indexOfProperty(it.key().toUtf8());
        if (index < 0)
            continue;
        QMetaProperty property = metaObject()->property(index);
        int notifyIndex = property.notifySignalIndex();
        if (notifyIndex >= 0)
            QMetaObject::activate(this, notifyIndex, nullptr);
    }
}

void QBackendObject::invokeMethod(const QByteArray& method, const QJSValue& data)
{
#if 0
    QJSEngine *engine = qmlEngine(this);
    QJSValue global = engine->globalObject();
    QJSValue json = global.property("JSON");
    QJSValue stringify = json.property("stringify");
    QJSValue jsonData = stringify.call(QList<QJSValue>() << data);
    m_connection->invokeMethod(m_identifier, method, jsonData.toString().toUtf8());
#endif
}

#if 0
        QJsonObject obj;
        for (auto it = changedProperties.constBegin(); it != changedProperties.constEnd(); it++) {
            qCDebug(lcObject) << "Requesting change on value " << it.key() << " on identifier " << m_identifier << " to " << it.value();
            obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
        }
        changedProperties.clear();
#endif

void QBackendObjectProxy::methodInvoked(const QString& method, const QJsonValue& params)
{
#if 0
    QJsonObject object = document.object();

    const QMetaObject *mo = metaObject();
    const int offset = mo->propertyOffset();
    const int count = mo->propertyCount();
    for (int i = offset; i < count; ++i) {
        QMetaProperty property = mo->property(i);

        const QVariant previousValue = readProperty(property);
        const QVariant currentValue = object.value(property.name()).toVariant();

        if (!currentValue.isNull() && (!previousValue.isValid()
                || (currentValue.canConvert(previousValue.type()) && previousValue != currentValue))) {
            qCDebug(lcObject) << "Got change on value " << property.name() << " on identifier " << m_identifier << " to " << currentValue;
            property.write(this, currentValue);
        }
    }
#endif
}
