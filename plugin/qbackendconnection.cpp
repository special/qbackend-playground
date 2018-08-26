#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QAbstractSocket>
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

        QAbstractSocket *rd = new QAbstractSocket(QAbstractSocket::UnknownSocketType, this);
        if (!rd->setSocketDescriptor(rdFd)) {
            qCritical() << "QBackendConnection failed for read fd:" << rd->errorString();
            return;
        }

        QAbstractSocket *wr = nullptr;
        if (rdFd == wrFd) {
            wr = rd;
        } else {
            wr = new QAbstractSocket(QAbstractSocket::UnknownSocketType, this);
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

QObject *QBackendConnection::rootObject()
{
    ensureRootObject();
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
            if (m_writeIo->write(data) < 0) {
                connectionError("flush pending data");
                return;
            }
        }
        m_pendingData.clear();
    }

    if (!m_blockReadSignals) {
        connect(m_readIo, &QIODevice::readyRead, this, &QBackendConnection::handleDataReady);
        handleDataReady();
    }
}

void QBackendConnection::blockReadSignals(bool v)
{
    if (v == m_blockReadSignals) {
        return;
    }
    m_blockReadSignals = v;

    if (v && m_readIo) {
        disconnect(m_readIo, &QIODevice::readyRead, this, &QBackendConnection::handleDataReady);
    } else if (!v && m_readIo) {
        connect(m_readIo, &QIODevice::readyRead, this, &QBackendConnection::handleDataReady);
        QMetaObject::invokeMethod(this, &QBackendConnection::handleDataReady, Qt::QueuedConnection);
    }
}

void QBackendConnection::moveToThread(QThread *thread)
{
    QObject::moveToThread(thread);
    if (m_readIo)
        m_readIo->moveToThread(thread);
    if (m_writeIo)
        m_writeIo->moveToThread(thread);
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

bool QBackendConnection::ensureConnectionInit()
{
    if (m_version)
        return true;
    if (!ensureConnectionConfig())
        return false;
    if (!m_readIo || !m_readIo->isOpen() || !m_writeIo || !m_writeIo->isOpen())
        return false;

    QElapsedTimer tm;
    qCDebug(lcConnection) << "Blocking until backend connection is ready";
    tm.restart();

    waitForMessage([](const QJsonObject &msg) { return msg.value("command").toString() == "VERSION"; });

    qCDebug(lcConnection) << "Blocked for" << tm.elapsed() << "ms to initialize connection";
    return m_version;
}

bool QBackendConnection::ensureRootObject()
{
    if (!ensureConnectionInit())
        return false;
    if (m_rootObject)
        return true;

    Q_ASSERT(qmlEngine());
    if (!qmlEngine()) {
        qCCritical(lcConnection) << "Connection cannot build root object without a QML engine";
        return false;
    }

    QElapsedTimer tm;
    qCDebug(lcConnection) << "Blocking until root object is ready";
    tm.restart();

    waitForMessage([](const QJsonObject &msg) { return msg.value("command").toString() == "ROOT"; });

    qCDebug(lcConnection) << "Blocked for" << tm.elapsed() << "ms for root object";
    return (bool)m_rootObject;
}

// Register instantiable types with the QML engine, blocking if necessary
void QBackendConnection::registerTypes(const char *uri)
{
    if (!ensureConnectionInit()) {
        qCCritical(lcConnection) << "Connection initialization failed, cannot register types";
        return;
    }

    // The only valid time to call this function is during a synchronous
    // connection, exactly once, so a CREATABLE_TYPES message cannot have been
    // handled before.
    Q_ASSERT(m_creatableTypes.isEmpty());

    QElapsedTimer tm;
    qCDebug(lcConnection) << "Blocking to initialize creatable types";
    tm.restart();

    waitForMessage([](const QJsonObject &msg) { return msg.value("command").toString() == "CREATABLE_TYPES"; });

    for (const QJsonValue &v : m_creatableTypes) {
        QJsonObject type = v.toObject();
        // See instantiable.h for an explanation of how this magic works
        if (!type.value("properties").toObject().value("_qb_model").isUndefined())
            addInstantiableBackendType<QBackendModel>(uri, this, type);
        else
            addInstantiableBackendType<QBackendObject>(uri, this, type);
    }

    qCDebug(lcConnection) << "Blocked for" << tm.elapsed() << "ms for creatable types";
}

void QBackendConnection::classBegin()
{
}

void QBackendConnection::componentComplete()
{
    // Block to wait for the connection to complete; this ensures that root is always
    // available, and avoids a lot of ugly initialization cases for applications.
    ensureRootObject();
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
 * RTFS. Backend is expected to send VERSION, CREATABLE_TYPES, and ROOT immediately, in
 * that order, unconditionally.
 */

void QBackendConnection::handleDataReady()
{
    int rdSize = m_readIo->bytesAvailable();
    if (rdSize < 1) {
        return;
    }

    int p = m_msgBuf.size();
    m_msgBuf.resize(p+rdSize);

    rdSize = m_readIo->read(m_msgBuf.data()+p, qint64(rdSize));
    if (rdSize < 0 || (rdSize == 0 && !m_readIo->isOpen())) {
        connectionError("read error");
        return;
    } else if (p+rdSize < m_msgBuf.size()) {
        m_msgBuf.resize(p+rdSize);
    }

    while (m_msgBuf.size() >= 2) {
        int headSz = m_msgBuf.indexOf(' ');
        if (headSz < 1) {
            if (headSz == 0) {
                // Everything has gone wrong
                qCDebug(lcConnection) << "Invalid data on connection:" << m_msgBuf;
                connectionError("invalid data");
            }
            // Otherwise, there's just not a full size yet
            return;
        }

        bool szOk = false;
        int blobSz = m_msgBuf.mid(0, headSz).toInt(&szOk);
        if (!szOk || blobSz < 1) {
            // Also everything has gone wrong
            qCDebug(lcConnection) << "Invalid data on connection:" << m_msgBuf;
            connectionError("invalid data");
            return;
        }
        // Include space in headSz now
        headSz++;

        // Wait for headSz + blobSz + 1 (the newline) bytes
        if (m_msgBuf.size() < headSz + blobSz + 1) {
            return;
        }

        // Skip past headSz, then read blob and trim newline
        QByteArray message = m_msgBuf.mid(headSz, blobSz);
        m_msgBuf.remove(0, headSz+blobSz+1);

        handleMessage(message);
    }
}

void QBackendConnection::connectionError(const QString &context)
{
    qCCritical(lcConnection) << "Connection failed during" << context <<
        ": (read: " << (m_readIo ? m_readIo->errorString() : "null") << ") "
        "(write: " << (m_writeIo ? m_writeIo->errorString() : "null") << ")";
    m_readIo->close();
    m_writeIo->close();
    qFatal("backend failed");
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
        connectionError("bad message");
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
        Q_ASSERT(!m_version);
        m_version = cmd.value("version").toInt();
        qCInfo(lcConnection) << "Connected to backend version" << m_version;
    } else if (command == "CREATABLE_TYPES") {
        m_creatableTypes = cmd.value("types").toArray();
    } else if (command == "ROOT") {
        // The cmd object itself is a backend object structure
        if (cmd.value("identifier").toString() != QStringLiteral("root")) {
            qCWarning(lcConnection) << "Root object has unexpected identifier:" << cmd.value("identifier");
            return;
        }

        if (!m_rootObject) {
            m_rootObject = ensureObject("root", cmd.value("type").toObject());
            QQmlEngine::setObjectOwnership(m_rootObject, QQmlEngine::CppOwnership);
            m_objects.value("root")->objectFound(cmd.value("data").toObject());
            emit ready();
        } else {
            // XXX assert that type has not changed
            m_objects.value("root")->objectFound(cmd.value("data").toObject());
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
    } else {
        qCWarning(lcConnection) << "Unknown command" << command << "from backend";
        connectionError("unknown command");
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
    if (m_writeIo->write(data) < 0) {
        connectionError("write");
    }
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
        if (!m_writeIo->waitForBytesWritten(5000)) {
            connectionError("synchronous write");
            return QJsonObject();
        }
    }

    Q_ASSERT(!m_syncCallback);
    m_syncCallback = callback;

    // When waitForMessage is called recursively from handleDataReady, make sure to
    // restore the syncResult before returning.
    auto savedResult = m_syncResult;
    m_syncResult = QJsonObject();

    // Flush pending messages, in case one of these is matched by the callback.
    // If not, they will be queued again because m_syncCallback is set.
    handlePendingMessages();

    while (m_syncResult.isEmpty()) {
        if (!m_readIo->waitForReadyRead(5000)) {
            connectionError("synchronous read");
            break;
        }
        handleDataReady();
    }

    QJsonObject re = m_syncResult;
    m_syncResult = savedResult;
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

void QBackendConnection::addObjectInstantiated(const QString &typeName, const QByteArray &identifier, QBackendRemoteObject *proxy)
{
    m_objects.insert(identifier, proxy);
    write(QJsonObject{
          {"command", "OBJECT_CREATE"},
          {"typeName", typeName},
          {"identifier", QString::fromUtf8(identifier)}
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
    return ensureObject(data.value("identifier").toString().toUtf8(), data.value("type").toObject());
}

QObject *QBackendConnection::ensureObject(const QByteArray &identifier, const QJsonObject &type)
{
    if (identifier.isEmpty())
        return nullptr;

    auto proxyObject = m_objects.value(identifier);
    if (!proxyObject) {
        QMetaObject *metaObject = newTypeMetaObject(type);
        QObject *object;

        if (metaObject->inherits(&QAbstractListModel::staticMetaObject))
            object = new QBackendModel(this, identifier, metaObject);
        else
            object = new QBackendObject(this, identifier, metaObject);
        QQmlEngine::setContextForObject(object, qmlContext(this));
        // This should be the result of the heuristic, but I never trust it.
        QQmlEngine::setObjectOwnership(object, QQmlEngine::JavaScriptOwnership);

        // Object constructor should have registered its proxy
        proxyObject = m_objects.value(identifier);
        Q_ASSERT(proxyObject);
    }

    return proxyObject->object();
}

QMetaObject *QBackendConnection::newTypeMetaObject(const QJsonObject &type)
{
    QMetaObject *mo = m_typeCache.value(type.value("name").toString());
    if (!mo) {
        if (type.value("omitted").toBool()) {
            // Type does not contain the full description, backend expected it to be cached.
            qCWarning(lcConnection) << "Expected cached type description for" << type.value("name").toString() << "to create object";
            // This is a bug, but allow it to continue as an object with no properties
        }

        // If type is a model type, set a superclass as well
        if (!type.value("properties").toObject().value("_qb_model").isUndefined()) {
            mo = metaObjectFromType(type, &QAbstractListModel::staticMetaObject);
        } else {
            mo = metaObjectFromType(type, nullptr);
        }

        m_typeCache.insert(type.value("name").toString(), mo);
        qDebug(lcConnection) << "Cached metaobject for type" << type.value("name").toString();
    }

    // Return a copy of the cached metaobject
    QMetaObjectBuilder b(mo);
    return b.toMetaObject();
}
