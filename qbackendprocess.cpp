#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>

#include "qbackendprocess.h"

Q_LOGGING_CATEGORY(lcProto, "backend.proto")
Q_LOGGING_CATEGORY(lcProtoExtreme, "backend.proto.extreme", QtWarningMsg)

QBackendProcess::QBackendProcess(QObject *parent)
    : QBackendAbstractConnection(parent)
    , m_completed(false)
{
}

QString QBackendProcess::name() const
{
    return m_name;
}

void QBackendProcess::setName(const QString& name)
{
    if (m_completed) {
        Q_UNREACHABLE();
        return;
    }

    m_name = name;
    emit nameChanged();
}

QStringList QBackendProcess::args() const
{
    return m_args;
}

void QBackendProcess::setArgs(const QStringList& args)
{
    if (m_completed) {
        Q_UNREACHABLE();
        return;
    }

    m_args = args;
    emit argsChanged();
}

void QBackendProcess::classBegin()
{

}

void QBackendProcess::componentComplete()
{
    m_completed = true;

    // Start the process
    m_process.start("go", QStringList() << "run" << "test.go");

    m_process.waitForStarted(); // ### kind of nasty
    if (m_pendingData.length()) {
        for (const QByteArray& data : m_pendingData) {
            write(data);
        }
        m_pendingData.clear();
    }

    connect(&m_process, &QIODevice::readyRead, this, &QBackendProcess::handleModelDataReady);
    handleModelDataReady();
}

QJsonDocument QBackendProcess::readJsonBlob(int byteCount)
{
    QByteArray cmdBuf;
    while (cmdBuf.length() < byteCount) {
        qCDebug(lcProtoExtreme) << "Want " << byteCount << " bytes, have " << cmdBuf.length();
        m_process.waitForReadyRead(10);
        cmdBuf += m_process.read(byteCount - cmdBuf.length());
    }
    Q_ASSERT(cmdBuf.length() == byteCount);
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(cmdBuf, &pe);
    if (doc.isNull()) {
        qWarning() << "Bad blob: " << cmdBuf << pe.errorString();
    }
    return doc;
}

void QBackendProcess::handleModelDataReady()
{
    while (m_process.canReadLine()) {
        qCDebug(lcProtoExtreme) << "Reading...";
        QByteArray cmdBuf = m_process.readLine();
        if (cmdBuf == "\n") {
            // ignore
            continue;
        }

        qCDebug(lcProto) << "Read " << cmdBuf;
        if (cmdBuf.startsWith("VERSION ")) {
            // First, remove the newline.
            cmdBuf.truncate(cmdBuf.length() - 1);
            QList<QByteArray> parts = cmdBuf.split(' ');
            Q_ASSERT(parts.length() == 2);
            qCInfo(lcProto) << "Connected to backend version " << parts[1];
        } else if (cmdBuf.startsWith("OBJECT_CREATE ")) {
            // First, remove the newline.
            cmdBuf.truncate(cmdBuf.length() - 1);

            QList<QByteArray> parts = cmdBuf.split(' ');
            Q_ASSERT(parts.length() == 2);
            QByteArray identifier = QByteArray(parts[1]);

            QJsonDocument doc = readJsonBlob(parts[2].toInt());
            if (m_subscribedObjects.contains(identifier)) {
                m_subscribedObjects[identifier]->objectFound(doc);
            } else {
                qCWarning(lcProto) << "Got creation for unsubscribed identifier: " << identifier << doc;
            }
        } else if (cmdBuf.startsWith("OBJECT_INVOKE ")) {
            // First, remove the newline.
            cmdBuf.truncate(cmdBuf.length() - 1);

            QList<QByteArray> parts = cmdBuf.split(' ');
            Q_ASSERT(parts.length() == 3);

            QJsonDocument doc = readJsonBlob(parts[3].toInt());
            qCDebug(lcProto) << "Invoke " << parts[2] << " on " << parts[1] << doc.toVariant();
        }
    }
}

void QBackendProcess::write(const QByteArray& data)
{
    if (m_process.state() != QProcess::Running) {
        qCDebug(lcProtoExtreme) << "Write on a non-running process buffered: " << data;
        m_pendingData.append(data);
        return;
    }

    qCDebug(lcProto) << "Writing " << data;
    m_process.write(data);
}

void QBackendProcess::invokeMethod(const QByteArray& identifier, const QString& method, const QByteArray& jsonData)
{
    qCDebug(lcProto) << "Invoking " << identifier << method << jsonData;
    QString data = "INVOKE " + identifier + " " + method + " " + QString::number(jsonData.length()) + "\n";
    write(data.toUtf8());
    write(jsonData + '\n');
}

// ### unsubscribe
void QBackendProcess::subscribe(const QByteArray& identifier, QBackendRemoteObject* object)
{
    qCDebug(lcProto) << "Creating remote object handler " << identifier << " on connection " << this << " for " << object;
    m_subscribedObjects[identifier] = object;
    QString data = "SUBSCRIBE " + identifier + "\n";
    write(data.toUtf8());
}


