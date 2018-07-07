#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QLocalSocket>
#include <QQmlEngine>
#include <QQmlContext>
#include <QCoreApplication>
#include <QElapsedTimer>

#include "qbackendconnection.h"
#include "qbackendobject.h"
#include "qbackendmodel.h"
#include "instantiable.h"

// #define PROTO_DEBUG

Q_LOGGING_CATEGORY(lcConnection, "backend.connection")
Q_LOGGING_CATEGORY(lcProto, "backend.proto")
Q_LOGGING_CATEGORY(lcProtoExtreme, "backend.proto.extreme", QtWarningMsg)

QBackendConnection::QBackendConnection(QObject *parent)
    : QObject(parent)
{
}

QBackendConnection::QBackendConnection(QQmlEngine *engine)
    : QObject()
    , m_qmlEngine(engine)
{
}

// When QBackendConnection is a singleton, qmlEngine/qmlContext may not always work.
// This will return the explicit engine as well, if one is known.
QQmlEngine *QBackendConnection::qmlEngine() const
{
    return m_qmlEngine ? m_qmlEngine : ::qmlEngine(this);
}

void QBackendConnection::setQmlEngine(QQmlEngine *engine)
{
    Q_ASSERT(!m_qmlEngine);
    if (m_qmlEngine) {
        qCritical(lcConnection) << "Backend connection is reused by another QML engine. This will go badly.";
        return;
    }

    m_qmlEngine = engine;
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

QBackendObject *QBackendConnection::rootObject()
{
    ensureConnectionReady();
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

bool QBackendConnection::ensureConnectionConfig()
{
    if (!m_url.isEmpty()) {
        return true;
    }

    // Try to setup connection from the QML context, cmdline, and environment, in that order
    QQmlContext *context = qmlContext(this);
    if (!context && m_qmlEngine) {
        context = m_qmlEngine->rootContext();
    }
    if (context) {
        QString url = context->contextProperty("qbackendUrl").toString();
        if (!url.isEmpty()) {
            qCDebug(lcConnection) << "Configuring connection URL from" << (qmlContext(this) ? "object" : "root") << "context property";
            setUrl(url);
            return true;
        }
    } else {
        qCDebug(lcConnection) << "No context associated with connection object, skipping context configuration";
    }

    QStringList args = QCoreApplication::arguments();
    int argp = args.indexOf("-qbackend");
    if (argp >= 0 && argp+1 < args.size()) {
        qCDebug(lcConnection) << "Configuring connection URL from commandline";
        setUrl(args[argp+1]);
        return true;
    }

    QString env = qEnvironmentVariable("QBACKEND_URL");
    if (!env.isEmpty()) {
        qCDebug(lcConnection) << "Configuring connection URL from environment";
        setUrl(env);
        return true;
    }

    return false;
}

bool QBackendConnection::ensureConnectionReady()
{
    if (m_rootObject)
        return true;
    if (!ensureConnectionConfig())
        return false;
    if (!m_readIo || !m_readIo->isOpen() || !m_writeIo || !m_writeIo->isOpen())
        return false;

    QElapsedTimer tm;
    qCDebug(lcConnection) << "Blocking until backend connection is ready";
    tm.restart();

    // Flush write buffer before blocking
    while (m_writeIo->bytesToWrite() > 0) {
        if (!m_writeIo->waitForBytesWritten(5000))
            break;
    }

    handleDataReady();
    while (!m_rootObject) {
        if (!m_readIo->waitForReadyRead(5000))
            break;
    }

    qCDebug(lcConnection) << "Blocked for" << tm.elapsed() << "ms to initialize connection";
    return true;
}

// Register instantiable types with the QML engine, blocking if necessary
void QBackendConnection::registerTypes(const char *uri)
{
}

void QBackendConnection::classBegin()
{
}

void QBackendConnection::componentComplete()
{
    // Block to wait for the connection to complete; this ensures that root is always
    // available, and avoids a lot of ugly initialization cases for applications.
    ensureConnectionReady();
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

void QBackendConnection::handleMessage(const QByteArray &message)
{
#if defined(PROTO_DEBUG)
    qCDebug(lcProto) << "Read " << message;
#endif

    QJsonParseError pe;
    QJsonDocument json = QJsonDocument::fromJson(message, &pe);
    if (!json.isObject()) {
        qCWarning(lcProto) << "bad message:" << message << pe.errorString();
        return;
    }

    handleMessage(json.object());
}

void QBackendConnection::handleMessage(const QJsonObject &cmd)
{
    QString command = cmd.value("command").toString();

    // If there is a syncCallback, call it to see if this is the message it wants.
    // If so, handle it normally, save it in m_syncResult, and return. If not,
    // queue it to handle later.
    //
    // If there is a syncResult, always queue the message.
    if ((m_syncCallback && !m_syncCallback(cmd)) || !m_syncResult.isEmpty()) {
        qCDebug(lcConnection) << "Queuing handling of unrelated" << command << "during waitForMessage";
        if (m_pendingMessages.isEmpty()) {
            QMetaObject::invokeMethod(this, &QBackendConnection::handlePendingMessages, Qt::QueuedConnection);
        }
        m_pendingMessages.append(cmd);
        return;
    }

    if (m_syncCallback) {
        // Reset callback, set result, and handle immediately, bypassing the queue
        // even if out of order; see waitForMessage for details.
        m_syncCallback = nullptr;
        m_syncResult = cmd;
    } else if (!m_pendingMessages.isEmpty()) {
        // Other messages are pending, so queue this one as well
        qCDebug(lcConnection) << "Queuing handling of" << command << "into non-empty message queue";
        // In this case, there's no need to trigger the handler later; something already has
        m_pendingMessages.append(cmd);
        return;
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
            m_rootObject = new QBackendObject(this, "root", cmd.value("type").toObject(), this);
            QQmlEngine::setContextForObject(m_rootObject, qmlContext(this));
            m_rootObject->resetData(cmd.value("data").toObject());
            emit ready();
        } else {
            // XXX assert that type has not changed
            m_rootObject->resetData(cmd.value("data").toObject());
        }
    } else if (command == "OBJECT_RESET") {
        QByteArray identifier = cmd.value("identifier").toString().toUtf8();
        auto obj = m_objects.value(identifier);
        if (obj) {
            obj->objectFound(cmd.value("data").toObject());
        }
    } else if (command == "EMIT") {
        QByteArray identifier = cmd.value("identifier").toString().toUtf8();
        QString method = cmd.value("method").toString();
        QJsonArray params = cmd.value("parameters").toArray();

        qCDebug(lcConnection) << "Emit " << method << " on " << identifier << params;
        auto obj = m_objects.value(identifier);
        if (obj) {
            obj->methodInvoked(method, params);
        }
    }
}

void QBackendConnection::handlePendingMessages()
{
    const auto pending = m_pendingMessages;
    m_pendingMessages.clear();
    if (pending.isEmpty()) {
        return;
    }

    qCDebug(lcConnection) << "Handling" << pending.size() << "queued messages";
    for (const QJsonObject &msg : pending) {
        handleMessage(msg);
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
//
// waitForMessage is safe to call recursively (for different messages), even if those messages
// arrive out of order.
QJsonObject QBackendConnection::waitForMessage(std::function<bool(const QJsonObject&)> callback)
{
    // Flush write buffer before blocking
    while (m_writeIo->bytesToWrite() > 0) {
        if (!m_writeIo->waitForBytesWritten(5000))
            break;
    }

    Q_ASSERT(!m_syncCallback);
    Q_ASSERT(m_syncResult.isEmpty());
    m_syncCallback = callback;

    // Flush pending messages, in case one of these is matched by the callback.
    // If not, they will be queued again because m_syncCallback is set.
    handlePendingMessages();

    while (m_syncResult.isEmpty()) {
        if (!m_readIo->waitForReadyRead(5000))
            break;
        handleDataReady();
    }

    QJsonObject re = m_syncResult;
    m_syncResult = QJsonObject();
    return re;
}

void QBackendConnection::invokeMethod(const QByteArray& identifier, const QString& method, const QJsonArray& params)
{
    qCDebug(lcConnection) << "Invoking " << identifier << method << params;
    write(QJsonObject{
          {"command", "INVOKE"},
          {"identifier", QString::fromUtf8(identifier)},
          {"method", method},
          {"parameters", params}
    });
}

void QBackendConnection::addObjectProxy(const QByteArray& identifier, QBackendRemoteObject* proxy)
{
    if (m_objects.contains(identifier)) {
        qCWarning(lcConnection) << "Duplicate object identifiers on connection for objects" << proxy << "and" << m_objects.value(identifier);
        return;
    }

    qCDebug(lcConnection) << "Creating remote object handler " << identifier << " on connection " << this << " for " << proxy;
    m_objects.insert(identifier, proxy);

    write(QJsonObject{
          {"command", "OBJECT_REF"},
          {"identifier", QString::fromUtf8(identifier)},
    });
}

void QBackendConnection::resetObjectData(const QByteArray& identifier, bool synchronous)
{
    write(QJsonObject{{"command", "OBJECT_QUERY"}, {"identifier", QString::fromUtf8(identifier)}});

    if (synchronous) {
        waitForMessage([identifier](const QJsonObject &message) -> bool {
            if (message.value("command").toString() != "OBJECT_RESET")
                return false;
            return message.value("identifier").toString().toUtf8() == identifier;
        });
    }
}

void QBackendConnection::removeObject(const QByteArray& identifier)
{
    if (!m_objects.contains(identifier)) {
        qCWarning(lcConnection) << "Removing object identifier" << identifier << "on connection" << this << "which isn't in list";
        return;
    }

    qCDebug(lcConnection) << "Removing remote object handler " << identifier << " on connection " << this << " for ";
    m_objects.remove(identifier);

    write(QJsonObject{
          {"command", "OBJECT_DEREF"},
          {"identifier", QString::fromUtf8(identifier)}
    });
}

QObject *QBackendConnection::object(const QByteArray &identifier) const
{
    auto obj = m_objects.value(identifier);
    if (obj)
        return obj->object();
    return nullptr;
}

// Create or return the backend object described by `object`, which is in the
// "_qbackend_": "object" format described in qbackendobject.cpp.
QObject *QBackendConnection::ensureObject(const QJsonObject &data)
{
    QByteArray identifier = data.value("identifier").toString().toUtf8();
    if (identifier.isEmpty())
        return nullptr;

    auto proxyObject = m_objects.value(identifier);
    if (!proxyObject) {
        QJsonObject type = data.value("type").toObject();

        QObject *object;
        if (!type.value("properties").toObject().value("_qb_model").isUndefined())
            object = new QBackendModel(this, identifier, type);
        else
            object = new QBackendObject(this, identifier, type);
        QQmlEngine::setContextForObject(object, qmlContext(this));
        // This should be the result of the heuristic, but I never trust it.
        QQmlEngine::setObjectOwnership(object, QQmlEngine::JavaScriptOwnership);

        // Object constructor should have registered its proxy
        proxyObject = m_objects.value(identifier);
        Q_ASSERT(proxyObject);
    }

    return proxyObject->object();
}
