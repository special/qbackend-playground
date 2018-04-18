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

#include "qbackendstore.h"
#include "qbackendabstractconnection.h"

Q_LOGGING_CATEGORY(lcStore, "backend.store")

QBackendStore::QBackendStore(QObject *parent)
    : QObject(parent)
{

}

QByteArray QBackendStore::identifier() const
{
    return m_identifier;
}

// ### error on componentComplete if not set
void QBackendStore::setIdentifier(const QByteArray& identifier)
{
    if (m_identifier == identifier) {
        return;
    }

    m_identifier = identifier;
    subscribeIfReady();
}

QBackendAbstractConnection* QBackendStore::connection() const
{
    return m_connection;
}

// ### error on componentComplete if not set
void QBackendStore::setConnection(QBackendAbstractConnection* connection)
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

QObject *QBackendStore::data() const
{
    return m_dataObject;
}

class QBackendStoreProxy : public QBackendRemoteObject
{
public:
    QBackendStoreProxy(QBackendStore* model);
    void objectFound(const QJsonDocument& document) override;
    void methodInvoked(const QByteArray& method, const QJsonDocument& document) override;

private:
    QBackendStore *m_store = nullptr;
};

QBackendStoreProxy::QBackendStoreProxy(QBackendStore* store)
    : m_store(store)
{

}

void QBackendStoreProxy::objectFound(const QJsonDocument& document)
{
    m_store->doReset(document);
}

void QBackendStore::doReset(const QJsonDocument& document)
{
    qCDebug(lcStore) << "Resetting " << m_identifier << " to " << document;
    if (m_dataObject) {
        m_dataObject->deleteLater();
    }

    if (!document.isObject()) {
        qCWarning(lcStore) << "Got a change that wasn't an object? " << document;
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
    myComp.setData(componentSource.toUtf8(), QUrl("qrc:/qbackendstore/" + m_identifier));
    m_dataObject = myComp.create();
    if (m_dataObject == nullptr) {
        qWarning(lcStore) << "Failed to create runtime object for " << m_identifier << componentSource.toUtf8().data();
        qWarning(lcStore) << myComp.errorString();
    }
    Q_ASSERT(m_dataObject);
    emit dataChanged();
}

void QBackendStore::invokeMethod(const QByteArray& method, const QJSValue& data)
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
            qCDebug(lcStore) << "Requesting change on value " << it.key() << " on identifier " << m_identifier << " to " << it.value();
            obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
        }
        changedProperties.clear();
#endif

void QBackendStoreProxy::methodInvoked(const QByteArray& method, const QJsonDocument& document)
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
            qCDebug(lcStore) << "Got change on value " << property.name() << " on identifier " << m_identifier << " to " << currentValue;
            property.write(this, currentValue);
        }
    }
#endif
}

void QBackendStore::subscribeIfReady()
{
    if (!m_connection || m_identifier.isEmpty()) {
        return;
    }

    Q_ASSERT(!m_proxy);

    m_proxy = new QBackendStoreProxy(this);
    m_connection->subscribe(m_identifier, m_proxy);
}

