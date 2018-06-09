#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QLocalSocket>
#include <QQmlEngine>

#include "qbackendconnection.h"
#include "qbackendobject.h"

// #define PROTO_DEBUG

Q_LOGGING_CATEGORY(lcConnection, "backend.connection")
Q_LOGGING_CATEGORY(lcProto, "backend.proto")
Q_LOGGING_CATEGORY(lcProtoExtreme, "backend.proto.extreme", QtWarningMsg)

QBackendConnection::QBackendConnection(QObject *parent)
    : QBackendAbstractConnection(parent)
{
}

QUrl QBackendConnection::url() const
{
    return m_url;
}

void QBackendConnection::setUrl(const QUrl& url)
{
    m_url = url;
    emit urlChanged();

    qCInfo(lcConnection) << "Opening URL" << url;

    if (url.scheme() == "fd") {
        // fd:0 (rw) or fd:0,1 (r,w)
        QStringList values = url.path().split(",");
        int rdFd = -1, wrFd = -1;
        bool ok = false;
        if (values.size() == 2) {
            rdFd = values[0].toInt(&ok);
            if (!ok)
                rdFd = -1;
            wrFd = values[1].toInt(&ok);
            if (!ok)
                wrFd = -1;
        } else if (values.size() == 1) {
            rdFd = wrFd = values[0].toInt(&ok);
            if (!ok)
                rdFd = wrFd = -1;
        }

        if (rdFd < 0 || wrFd < 0) {
            qCritical() << "Invalid QBackendConnection url" << url;
            return;
        }

        QLocalSocket *rd = new QLocalSocket(this);
        if (!rd->setSocketDescriptor(rdFd)) {
            qCritical() << "QBackendConnection failed for read fd:" << rd->errorString();
            return;
        }

        QLocalSocket *wr = nullptr;
        if (rdFd == wrFd) {
            wr = rd;
        } else {
            wr = new QLocalSocket(this);
            if (!wr->setSocketDescriptor(wrFd)) {
                qCritical() << "QBackendConnection failed for write fd:" << wr->errorString();
                return;
            }
        }

        setBackendIo(rd, wr);
    } else {
        qCritical() << "Unknown QBackendConnection scheme" << url.scheme();
        return;
    }
}

void QBackendConnection::setBackendIo(QIODevice *rd, QIODevice *wr)
{
    if (m_readIo || m_writeIo) {
        qFatal("QBackendConnection IO cannot be reset");
        return;
    }

    m_readIo = rd;
    m_writeIo = wr;

    if (m_pendingData.length()) {
        for (const QByteArray& data : m_pendingData) {
            write(data);
        }
        m_pendingData.clear();
    }

    connect(m_readIo, &QIODevice::readyRead, this, &QBackendConnection::handleModelDataReady);
    handleModelDataReady();
}

QJsonDocument QBackendConnection::readJsonBlob(int byteCount)
{
    QByteArray cmdBuf;
    while (cmdBuf.length() < byteCount) {
        qCDebug(lcProtoExtreme) << "Want " << byteCount << " bytes, have " << cmdBuf.length();
        m_readIo->waitForReadyRead(10);
        cmdBuf += m_readIo->read(byteCount - cmdBuf.length());
    }
    Q_ASSERT(cmdBuf.length() == byteCount);
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(cmdBuf, &pe);
    if (doc.isNull()) {
        qCWarning(lcProto) << "Bad blob: " << cmdBuf << pe.errorString();
    }
    return doc;
}

void QBackendConnection::handleModelDataReady()
{
    while (m_readIo->canReadLine()) {
        qCDebug(lcProtoExtreme) << "Reading...";
        QByteArray cmdBuf = m_readIo->readLine();
        if (cmdBuf == "\n") {
            // ignore
            continue;
        }

#if defined(PROTO_DEBUG)
        qCDebug(lcProto) << "Read " << cmdBuf;
#endif
        if (cmdBuf.startsWith("VERSION ")) {
            // First, remove the newline.
            cmdBuf.truncate(cmdBuf.length() - 1);
            QList<QByteArray> parts = cmdBuf.split(' ');
            Q_ASSERT(parts.length() == 2);
            qCInfo(lcConnection) << "Connected to backend version " << parts[1];
        } else if (cmdBuf.startsWith("OBJECT_CREATE ")) {
            // First, remove the newline.
            cmdBuf.truncate(cmdBuf.length() - 1);

            QList<QByteArray> parts = cmdBuf.split(' ');
            Q_ASSERT(parts.length() == 2);
            QByteArray identifier = QByteArray(parts[1]);

            QJsonDocument doc = readJsonBlob(parts[2].toInt());
            for (auto obj : m_subscribedObjects.values(identifier)) {
                obj->objectFound(doc);
            }
        } else if (cmdBuf.startsWith("EMIT ")) {
            // First, remove the newline.
            cmdBuf.truncate(cmdBuf.length() - 1);

            QList<QByteArray> parts = cmdBuf.split(' ');
            Q_ASSERT(parts.length() == 3);
            QByteArray identifier = QByteArray(parts[1]);

            QJsonDocument doc = readJsonBlob(parts[3].toInt());

            qCDebug(lcConnection) << "Emit " << parts[2] << " on " << parts[1] << doc.toVariant();
            for (auto obj : m_subscribedObjects.values(identifier)) {
                obj->methodInvoked(parts[2], doc);
            }
        }
    }
}

void QBackendConnection::write(const QByteArray& data)
{
    if (!m_writeIo) {
        qCDebug(lcProtoExtreme) << "Write on an inactive connection buffered: " << data;
        m_pendingData.append(data);
        return;
    }

#if defined(PROTO_DEBUG)
    qCDebug(lcProto) << "Writing " << data;
#endif
    m_writeIo->write(data);
}

void QBackendConnection::invokeMethod(const QByteArray& identifier, const QString& method, const QByteArray& jsonData)
{
    qCDebug(lcConnection) << "Invoking " << identifier << method << jsonData;
    QString data = "INVOKE " + identifier + " " + method + " " + QString::number(jsonData.length()) + "\n";
    write(data.toUtf8());
    write(jsonData + '\n');
}

void QBackendConnection::subscribe(const QByteArray& identifier, QBackendRemoteObject* object)
{
    qCDebug(lcConnection) << "Creating remote object handler " << identifier << " on connection " << this << " for " << object;
    m_subscribedObjects.insert(identifier, object);
    QString data = "SUBSCRIBE " + identifier + "\n";
    write(data.toUtf8());
}

void QBackendConnection::unsubscribe(const QByteArray& identifier, QBackendRemoteObject* object)
{
    qCDebug(lcConnection) << "Removing remote object handler " << identifier << " on connection " << this << " for " << object;
    m_subscribedObjects.remove(identifier, object);
    QString data = "UNSUBSCRIBE " + identifier + "\n";
    write(data.toUtf8());
}

QBackendObject *QBackendConnection::object(const QByteArray &identifier)
{
    QPointer<QBackendObject> object = m_objects.value(identifier);
    if (!object) {
        object = new QBackendObject(this, identifier);
        m_objects.insert(identifier, object);
        QQmlEngine::setContextForObject(object, qmlContext(this));
        // This should be the result of the heuristic, but I never trust it.
        QQmlEngine::setObjectOwnership(object, QQmlEngine::JavaScriptOwnership);
    }
    return object;
}
