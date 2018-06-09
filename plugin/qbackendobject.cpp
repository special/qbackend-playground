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

#include "qbackendobject.h"
#include "qbackendabstractconnection.h"

Q_LOGGING_CATEGORY(lcObject, "backend.object")

class QBackendObjectProxy : public QBackendRemoteObject
{
public:
    QBackendObjectProxy(QBackendObject* model);
    void objectFound(const QJsonDocument& document) override;
    void methodInvoked(const QByteArray& method, const QJsonDocument& document) override;

private:
    QBackendObject *m_object = nullptr;
};

QBackendObject::QBackendObject(QBackendAbstractConnection *connection, QByteArray identifier, QObject *parent)
    : QObject(parent)
    , m_identifier(identifier)
    , m_connection(connection)
{
    m_proxy = new QBackendObjectProxy(this);
    m_connection->subscribe(m_identifier, m_proxy);
}

// XXX destructor?

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

QObject *QBackendObject::data() const
{
    return m_dataObject;
}

QBackendObjectProxy::QBackendObjectProxy(QBackendObject* object)
    : m_object(object)
{

}

void QBackendObjectProxy::objectFound(const QJsonDocument& document)
{
    if (!document.isObject()) {
        qCWarning(lcObject) << "Got a change that wasn't an object? " << document;
        return;
    }
    m_object->doReset(document.object());
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
    if (m_dataObject) {
        m_dataObject->deleteLater();
    }

    QString componentSource;
    componentSource  = "import QtQuick 2.0\n";
    componentSource += "QtObject {\n";
    // Assigned during creation below
    componentSource += "property var qbConnection\n";

    for (QJsonObject::const_iterator objit = object.constBegin(); objit != object.constEnd(); objit++) {
        QString type;
        QString name = objit.key();
        QString val;
        if (objit.value().isArray()) {
            type = "var";
            val = ": " + QJsonDocument(objit.value().toArray()).toJson(QJsonDocument::Compact);
            if (val == ": ") {
                val = ": []";
            }
        } else if (objit.value().isBool()) {
            type = "bool";
            if (objit.value().toBool()) {
                val = ": true";
            } else {
                val = ": false";
            }
        } else if (objit.value().isDouble()) {
            type = "double";
            val = ": " + QString::number(objit.value().toDouble());
        } else if (objit.value().isNull()) {
            type = "var";
            val = ": null";
        } else if (objit.value().isObject() && objit.value().toObject().value("_qbackend_").toString() == "object") {
            QJsonObject obj = objit.value().toObject();
            QString identifier = obj.value("identifier").toString();
            if (identifier.isEmpty() || identifier == m_identifier) {
                qCWarning(lcObject) << "Ignoring invalid object identifier in property:" << identifier;
                continue;
            }

            // Creates an object if necessary, otherwise reuses and updates data
            QBackendObject *backendObject = m_connection->object(identifier.toUtf8());
            backendObject->doReset(obj.value("data").toObject());

            type = "var";
            // XXX XXX Escape this! JSON escaping would be safe and reasonable, but no obvious way to get it.
            val = QStringLiteral(": qbConnection.object(\"%1\")").arg(identifier);
        } else if (objit.value().isObject()) {
            type = "var";
            val = ": " + QJsonDocument(objit.value().toObject()).toJson(QJsonDocument::Compact);
            if (val == ": ") {
                val = ": {}";
            }
        } else if (objit.value().isString()) {
            type = "string";
            val = ": \"" + objit.value().toString() + "\"";
        } else if (objit.value().isUndefined()) {
            type = "var";
            val = ": undefined";
        } else {
            Q_ASSERT("unknown type");
            continue;
        }

        componentSource += QString::fromLatin1("\treadonly property %1 %2%3\n").arg(type, name, val);
    }

    componentSource += "}\n";
    QQmlComponent myComp(qmlEngine(this));
    myComp.setData(componentSource.toUtf8(), QUrl("qrc:/qbackendobject/" + m_identifier));
    m_dataObject = myComp.beginCreate(qmlContext(this));
    if (m_dataObject == nullptr) {
        qWarning(lcObject) << "Failed to create runtime object for " << m_identifier << componentSource.toUtf8().data();
        qWarning(lcObject) << myComp.errorString();
        return;
    }
    m_dataObject->setProperty("qbConnection", QVariant::fromValue<QObject*>(m_connection));
    myComp.completeCreate();
    emit dataChanged();
}

void QBackendObject::invokeMethod(const QByteArray& method, const QJSValue& data)
{
    QJSEngine *engine = qmlEngine(this);
    QJSValue global = engine->globalObject();
    QJSValue json = global.property("JSON");
    QJSValue stringify = json.property("stringify");
    QJSValue jsonData = stringify.call(QList<QJSValue>() << data);
    m_connection->invokeMethod(m_identifier, method, jsonData.toString().toUtf8());
}

#if 0
        QJsonObject obj;
        for (auto it = changedProperties.constBegin(); it != changedProperties.constEnd(); it++) {
            qCDebug(lcObject) << "Requesting change on value " << it.key() << " on identifier " << m_identifier << " to " << it.value();
            obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
        }
        changedProperties.clear();
#endif

void QBackendObjectProxy::methodInvoked(const QByteArray& method, const QJsonDocument& document)
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
