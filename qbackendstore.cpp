#include <QDebug>
#include <QMetaObject>
#include <QMetaProperty>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSValue>
#include <QLoggingCategory>

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


void QBackendStore::classBegin()
{

}

void QBackendStore::componentComplete()
{
    // setup change notifications on first load
    const QMetaObject *mo = metaObject();
    const int offset = mo->propertyOffset();
    const int count = mo->propertyCount();
    for (int i = offset; i < count; ++i) {
        QMetaProperty property = mo->property(i);

        static const int propertyChangedIndex = mo->indexOfSlot("onPropertyChanged()");
        QMetaObject::connect(this, property.notifySignalIndex(), this, propertyChangedIndex);
    }
}

QVariant QBackendStore::readProperty(const QMetaProperty& property)
{
    QVariant var = property.read(this);
    if (var.userType() == qMetaTypeId<QJSValue>())
        var = var.value<QJSValue>().toVariant();
    return var;
}

void QBackendStore::onPropertyChanged()
{
    const QMetaObject *mo = metaObject();
    const int offset = mo->propertyOffset();
    const int count = mo->propertyCount();
    for (int i = offset; i < count; ++i) {
        const QMetaProperty &property = mo->property(i);
        const QVariant value = readProperty(property);
        changedProperties.insert(property.name(), value);
    }
    if (m_timerId == 0)
        m_timerId = startTimer(0);
}

void QBackendStore::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
        QJsonObject obj;
        for (auto it = changedProperties.constBegin(); it != changedProperties.constEnd(); it++) {
            qCDebug(lcStore) << "Requesting change on value " << it.key() << " on identifier " << m_identifier << " to " << it.value();
            obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
        }
        m_connection->invokeMethod(m_identifier, "set", QJsonDocument(obj).toJson(QJsonDocument::Compact));
        changedProperties.clear();
    }
    QObject::timerEvent(event);
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
    if (!document.isObject()) {
        qCWarning(lcStore) << "Got a change that wasn't an object? " << document;
        return;
    }

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
}

void QBackendStoreProxy::methodInvoked(const QByteArray& method, const QJsonDocument& document)
{

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

