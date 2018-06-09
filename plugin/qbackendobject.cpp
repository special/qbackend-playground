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

QBackendObject::QBackendObject(QObject *parent)
    : QObject(parent)
{

}

QByteArray QBackendObject::identifier() const
{
    return m_identifier;
}

// ### error on componentComplete if not set
void QBackendObject::setIdentifier(const QByteArray& identifier)
{
    if (m_identifier == identifier) {
        return;
    }

    m_identifier = identifier;
    subscribeIfReady();
}

QBackendAbstractConnection* QBackendObject::connection() const
{
    return m_connection;
}

// ### error on componentComplete if not set
void QBackendObject::setConnection(QBackendAbstractConnection* connection)
{
    if (connection == m_connection) {
        return;
    }

    m_connection = connection;
    if (!m_identifier.isEmpty()) {
        subscribeIfReady();
    }
    emit connectionChanged();
}

QObject *QBackendObject::data() const
{
    return m_dataObject;
}

class QBackendObjectProxy : public QBackendRemoteObject
{
public:
    QBackendObjectProxy(QBackendObject* model);
    void objectFound(const QJsonDocument& document) override;
    void methodInvoked(const QByteArray& method, const QJsonDocument& document) override;

private:
    QBackendObject *m_object = nullptr;
};

QBackendObjectProxy::QBackendObjectProxy(QBackendObject* object)
    : m_object(object)
{

}

void QBackendObjectProxy::objectFound(const QJsonDocument& document)
{
    m_object->doReset(document);
}

void QBackendObject::doReset(const QJsonDocument& document)
{
    qCDebug(lcObject) << "Resetting " << m_identifier << " to " << document;
    if (m_dataObject) {
        m_dataObject->deleteLater();
    }

    if (!document.isObject()) {
        qCWarning(lcObject) << "Got a change that wasn't an object? " << document;
        return;
    }
    QJsonObject object = document.object();
    QString componentSource;
    componentSource  = "import QtQuick 2.0\n";
    componentSource += "QtObject {\n";

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
    m_dataObject = myComp.create();
    if (m_dataObject == nullptr) {
        qWarning(lcObject) << "Failed to create runtime object for " << m_identifier << componentSource.toUtf8().data();
        qWarning(lcObject) << myComp.errorString();
    }
    Q_ASSERT(m_dataObject);
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

void QBackendObject::subscribeIfReady()
{
    if (!m_connection || m_identifier.isEmpty()) {
        return;
    }

    Q_ASSERT(!m_proxy);

    m_proxy = new QBackendObjectProxy(this);
    m_connection->subscribe(m_identifier, m_proxy);
}

