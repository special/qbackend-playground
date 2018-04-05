#include <iostream>

#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "qbackendmodel.h"
#include "qbackendprocess.h"

QBackendProcess::QBackendProcess(QObject *parent)
    : QObject(parent)
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

    // And handshake it to get it fully initialized. This does not really need
    // to be synchronous...
    m_process.waitForReadyRead();
    bool synced = false;
    while (!synced) {
        while (!m_process.canReadLine()) {
            // busy loop until we have a full line...
            m_process.waitForReadyRead();
        }
        QByteArray initBuf = m_process.readLine();
        initBuf.truncate(initBuf.length() - 1);

        if (initBuf.startsWith("VERSION")) {
            qDebug() << "Reading from " << initBuf;
        } else if (initBuf.startsWith("MODEL ")) {
            QList<QByteArray> modelData = initBuf.split(' ');
            Q_ASSERT(modelData.size() >= 2);
        } else if (initBuf == "SYNCED") {
            qDebug() << "Initial sync done";
            synced = true;
            break;
        } else {
            qWarning() << "Unknown initial burst " << initBuf;
            Q_UNREACHABLE();
        }
    }

    Q_ASSERT(synced);
    connect(&m_process, &QIODevice::readyRead, this, &QBackendProcess::handleModelDataReady);
    handleModelDataReady();
}

void QBackendProcess::handleModelDataReady()
{
    while (m_process.canReadLine()) {
        QByteArray cmdBuf = m_process.readLine();
        if (cmdBuf == "\n") {
            // ignore
            continue;
        }

        std::cout << "Read " << cmdBuf.data() << std::endl;
        if (cmdBuf.startsWith("APPEND ")) {
            // First, remove the newline.
            cmdBuf.truncate(cmdBuf.length() - 1);

            // APPEND <model> <UUID> <len>
            QList<QByteArray> parts = cmdBuf.split(' ');
            Q_ASSERT(parts.length() == 4);
            QUuid uuid = QUuid(parts[2]);
            int byteCnt = parts[3].toInt();

            // Read JSON data
            cmdBuf.clear();
            while (cmdBuf.length() < byteCnt) {
                qDebug() << "Want " << byteCnt << " bytes, have " << cmdBuf.length();
                m_process.waitForReadyRead(10);
                cmdBuf += m_process.read(byteCnt - cmdBuf.length());
            }
            Q_ASSERT(cmdBuf.length() == byteCnt);

            QJsonDocument doc = QJsonDocument::fromJson(cmdBuf);
            if (doc.isObject()) {
                QString modelId = QString::fromUtf8(parts[1]); // ### use QByteArray consistently
                QBackendModel *model = fetchModel(modelId);
                QJsonObject obj = doc.object();

                QBackendModel::QBackendRowData data;

                for (const QString& key : obj.keys()) {
                    data[key.toUtf8()] = obj.value(key).toVariant();
                }
                qDebug() << "Processing APPEND " << uuid << " into " << modelId << " len " << byteCnt << cmdBuf;
                model->appendFromProcess(QVector<QUuid>() << uuid, QVector<QBackendModel::QBackendRowData>() << data);
            } else {
                Q_UNREACHABLE(); // consider isArray for appending in bulk
            }
        } else if (cmdBuf.startsWith("REMOVE ")) {
            // First, remove the newline.
            cmdBuf.truncate(cmdBuf.length() - 1);

            // REMOVE <model> <UUID>
            QList<QByteArray> parts = cmdBuf.split(' ');

            Q_ASSERT(parts.length() == 3);

            QString modelId = QString::fromUtf8(parts[1]); // ### use QByteArray consistently
            QBackendModel *model = fetchModel(modelId);
            QUuid uuid = QUuid(parts[2]);
            model->removeFromProcess(QVector<QUuid>() << uuid);
        } else if (cmdBuf.startsWith("UPDATE ")) {
            // First, remove the newline.
            cmdBuf.truncate(cmdBuf.length() - 1);

            // UPDATE <model> <UUID> <len>
            QList<QByteArray> parts = cmdBuf.split(' ');

            Q_ASSERT(parts.length() == 3);

            QString modelId = QString::fromUtf8(parts[1]); // ### use QByteArray consistently
            QBackendModel *model = fetchModel(modelId);
            QUuid uuid = QUuid(parts[2]);

            int byteCnt = parts[3].toInt();

            // Read JSON data
            cmdBuf.clear();
            while (cmdBuf.length() < byteCnt) {
                qDebug() << "Want " << byteCnt << " bytes, have " << cmdBuf.length();
                m_process.waitForReadyRead(10);
                cmdBuf += m_process.read(byteCnt - cmdBuf.length());
            }
            Q_ASSERT(cmdBuf.length() == byteCnt);

            QJsonDocument doc = QJsonDocument::fromJson(cmdBuf);
            if (doc.isObject()) {
                QString modelId = QString::fromUtf8(parts[1]); // ### use QByteArray consistently
                QBackendModel *model = fetchModel(modelId);
                QJsonObject obj = doc.object();

                QBackendModel::QBackendRowData data;

                for (const QString& key : obj.keys()) {
                    data[key.toUtf8()] = obj.value(key).toVariant();
                }
                qDebug() << "Processing UPDATE " << uuid << " into " << modelId << " len " << byteCnt << cmdBuf;
                model->updateFromProcess(QVector<QUuid>() << uuid, QVector<QBackendModel::QBackendRowData>() << data);
            } else {
                Q_UNREACHABLE(); // consider isArray for appending in bulk
            }
        }
    }
}

void QBackendProcess::write(const QByteArray& data)
{
    if (data != "\n") {
        std::cout << "Sending " << data.data();
    }
    if (data[data.length() - 1] != '\n') {
        std::cout << std::endl;
    }
    m_process.write(data);
}

void QBackendProcess::invokeMethod(const QString& identifier, const QString& method, const QByteArray& jsonData)
{
    QString data = "INVOKE " + identifier + " " + method + " " + QString::number(jsonData.length()) + "\n";
    write(data.toUtf8());
    write(jsonData);
    write("\n");
}

void QBackendProcess::invokeMethodOnObject(const QString& identifier, const QUuid& id, const QString& method, const QByteArray& jsonData)
{
    QString data = "OINVOKE " + identifier + " " + method + " " + id.toByteArray() + " " + QString::number(jsonData.length()) + "\n";
    write(data.toUtf8());
    write(jsonData);
    write("\n");
}

QBackendModel* QBackendProcess::fetchModel(const QString& identifier)
{
    if (m_models.contains(identifier)) {
        return m_models[identifier];
    }

    qDebug() << "Creating model " << identifier << " on connection " << this;
    m_models[identifier] = new QBackendModel(this, identifier);
    return m_models[identifier];
}

