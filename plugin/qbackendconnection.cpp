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

QBackendObject *QBackendConnection::rootObject() const
{
    return m_rootObject;
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
            m_writeIo->write(data);
        }
        m_pendingData.clear();
    }

    connect(m_readIo, &QIODevice::readyRead, this, &QBackendConnection::handleDataReady);
    handleDataReady();
}

/* I gift to you a brief, possibly accurate protocol description.
 *
 * == Protocol framing ==
 * All messages begin with an ASCII-encoded integer greater than 0, followed by a space.
 * This is followed by a message blob of exactly that size, then by a newline (which is not
 * included in the blob size). That is:
 *
 *   "<int:size> <blob(size):message>\n"
 *
 * The message blob can contain newlines, so don't try to parse based on those.
 *
 * == Messages ==
 * Messages themselves are JSON objects. The only mandatory field is "command", all others
 * are command specific.
 *
 *   { "command": "VERSION", ... }
 *
 * == Commands ==
 * RTFS. Backend is expected to send VERSION and ROOT immediately, in that order.
 */

void QBackendConnection::handleDataReady()
{
    for (;;) {
        // Peek to see the (ASCII) integer size.
        // 11 bytes is enough for 2^32 and a space, which is way too much.
        QByteArray peek(11, 0);
        int peekSz = m_readIo->peek(peek.data(), peek.size());
        if (peekSz < 0 || (peekSz == 0 && !m_readIo->isOpen())) {
            qCWarning(lcConnection) << "Read error:" << m_readIo->errorString();
            return;
        }
        peek.resize(peekSz);

        int headSz = peek.indexOf(' ');
        if (headSz < 1) {
            if (headSz == 0 || peek.size() == 11) {
                // Everything has gone wrong
                qCWarning(lcConnection) << "Invalid data on connection:" << peek;
                // XXX All of these close/failure cases are basically unhandled.
                m_readIo->close();
            }
            // Otherwise, there's just not a full size yet
            return;
        }

        bool szOk = false;
        int blobSz = peek.mid(0, headSz).toInt(&szOk);
        if (!szOk || blobSz < 1) {
            // Also everything has gone wrong
            qCWarning(lcConnection) << "Invalid data on connection:" << peek;
            m_readIo->close();
            return;
        }
        // Include space in headSz now
        headSz++;

        // Wait for headSz + blobSz + 1 (the newline) bytes
        if (m_readIo->bytesAvailable() < headSz + blobSz + 1) {
            return;
        }

        // Skip past headSz, then read blob and trim newline
        m_readIo->skip(headSz);
        QByteArray message(blobSz + 1, 0);
        if (m_readIo->read(message.data(), message.size()) < message.size()) {
            // This should not happen, unless bytesAvailable lied
            qCWarning(lcConnection) << "Read failed:" << m_readIo->errorString();
            return;
        }
        message.chop(1);

        handleMessage(message);
    }
}

void QBackendConnection::handleMessage(const QByteArray &message) {
#if defined(PROTO_DEBUG)
    qCDebug(lcProto) << "Read " << message;
#endif

    QJsonParseError pe;
    QJsonDocument json = QJsonDocument::fromJson(message, &pe);
    if (!json.isObject()) {
        qCWarning(lcProto) << "bad message:" << message << pe.errorString();
        return;
    }
    QJsonObject cmd = json.object();
    QString command = cmd.value("command").toString();

    // If there is a syncCallback, call it to see if this is the message it wants.
    // If so, handle it normally, save it in m_syncResult, and return. If not,
    // queue it to handle later.
    //
    // If there is a syncResult, always queue the message.
    if ((m_syncCallback && !m_syncCallback(cmd)) || !m_syncResult.isEmpty()) {
        qCDebug(lcConnection) << "Queuing handling of unrelated message during waitForMessage";
        QMetaObject::invokeMethod(this, [=]() { handleMessage(message); }, Qt::QueuedConnection);
        return;
    }
    if (m_syncCallback) {
        m_syncCallback = nullptr;
        m_syncResult = cmd;
    }

    if (command == "VERSION") {
        qCInfo(lcConnection) << "Connected to backend version " << cmd.value("version");
    } else if (command == "ROOT") {
        // The cmd object itself is a backend object structure
        if (cmd.value("identifier").toString() != QStringLiteral("root")) {
            qCWarning(lcConnection) << "Root object has unexpected identifier:" << cmd.value("identifier");
            return;
        }

        if (!m_rootObject) {
            m_rootObject = new QBackendObject(this, "root", this);
            m_objects.insert("root", m_rootObject);
            QQmlEngine::setContextForObject(m_rootObject, qmlContext(this));
            m_rootObject->doReset(cmd.value("data").toObject());
            emit rootObjectChanged();
        } else {
            m_rootObject->doReset(cmd.value("data").toObject());
        }
    } else if (command == "OBJECT_CREATE") {
        QByteArray identifier = cmd.value("identifier").toString().toUtf8();

        for (auto obj : m_subscribedObjects.values(identifier)) {
            obj->objectFound(cmd.value("data").toObject());
        }
    } else if (command == "EMIT") {
        QByteArray identifier = cmd.value("identifier").toString().toUtf8();
        QString method = cmd.value("method").toString();
        QJsonValue params = cmd.value("parameters");

        qCDebug(lcConnection) << "Emit " << method << " on " << identifier << params;
        for (auto obj : m_subscribedObjects.values(identifier)) {
            obj->methodInvoked(method, params);
        }
    }
}

void QBackendConnection::write(const QJsonObject &message)
{
    QByteArray data = QJsonDocument(message).toJson(QJsonDocument::Compact);
    data = QByteArray::number(data.size()) + " " + data + "\n";

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

// waitForMessage blocks and reads messages from the connection, passing each to the callback
// function until it returns true. The selected message is returned.
//
// Any other messages (returning false from the callback) will be queued to handle normally
// later. They will not have been handled when this function returns; the selected message is
// taken out of order.
QJsonObject QBackendConnection::waitForMessage(std::function<bool(const QJsonObject&)> callback)
{
    Q_ASSERT(!m_syncCallback);
    Q_ASSERT(m_syncResult.isEmpty());
    m_syncCallback = callback;
    while (m_syncResult.isEmpty()) {
        m_readIo->waitForReadyRead(5000);
    }
    QJsonObject re = m_syncResult;
    m_syncResult = QJsonObject();
    return re;
}

void QBackendConnection::invokeMethod(const QByteArray& identifier, const QString& method, const QByteArray& jsonData)
{
    qCDebug(lcConnection) << "Invoking " << identifier << method << jsonData;

    // It seems silly to encode and then decode this JSON just to reencode it, but that first encoding
    // is done by JSON.stringify, which handles QJSValue correctly. This is the easiest way to work with it.
    QJsonDocument params = QJsonDocument::fromJson(jsonData);

    write(QJsonObject{
          {"command", "INVOKE"},
          {"identifier", QString::fromUtf8(identifier)},
          {"parameters", params.array()}
    });
}

void QBackendConnection::subscribe(const QByteArray& identifier, QBackendRemoteObject* object)
{
    qCDebug(lcConnection) << "Creating remote object handler " << identifier << " on connection " << this << " for " << object;
    m_subscribedObjects.insert(identifier, object);
    write(QJsonObject{{"command", "SUBSCRIBE"}, {"identifier", QString::fromUtf8(identifier)}});
}

void QBackendConnection::unsubscribe(const QByteArray& identifier, QBackendRemoteObject* object)
{
    qCDebug(lcConnection) << "Removing remote object handler " << identifier << " on connection " << this << " for " << object;
    m_subscribedObjects.remove(identifier, object);
    write(QJsonObject{{"command", "UNSUBSCRIBE"}, {"identifier", QString::fromUtf8(identifier)}});
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
