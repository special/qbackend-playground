#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QTimerEvent>
#include <QHash>
#include <QVariant>

class QMetaProperty;
class QBackendStoreProxy;
#include "qbackendabstractconnection.h"

class QBackendStore : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_PROPERTY(QByteArray identifier READ identifier WRITE setIdentifier NOTIFY identifierChanged)
    Q_PROPERTY(QBackendAbstractConnection* connection READ connection WRITE setConnection NOTIFY connectionChanged)
public:
    QBackendStore(QObject *parent = 0);

    QByteArray identifier() const;
    void setIdentifier(const QByteArray& identifier);

    QBackendAbstractConnection* connection() const;
    void setConnection(QBackendAbstractConnection* connection);

    // ### not public
    void doReset(const QJsonDocument& document);

protected:
    void classBegin() override;
    void componentComplete() override;

private slots:
    void onPropertyChanged();
    void timerEvent(QTimerEvent *event);

signals:
    void identifierChanged();
    void connectionChanged();

private:
    QVariant readProperty(const QMetaProperty& property);
    void subscribeIfReady();

    int m_timerId = 0;
    QByteArray m_identifier;
    QBackendAbstractConnection *m_connection = nullptr;
    QBackendStoreProxy *m_proxy = nullptr;
    QHash<const char *, QVariant> changedProperties;
};


