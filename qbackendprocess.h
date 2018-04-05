#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QUuid>
#include <QProcess>

// A backend process is one form of RPC. It is not the only form of RPC.
// It populates the repository with properties, models, and so on.

class QBackendProcess : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QStringList args READ args WRITE setArgs NOTIFY argsChanged)
public:
    QBackendProcess(QObject *parent = 0);

    QString name() const;
    void setName(const QString& name);

    QStringList args() const;
    void setArgs(const QStringList& args);

protected:
    void classBegin() override;
    void componentComplete() override;

public:
    // #### these eventually should be in a base class
    // Requests that the backend add a new item.
    void add(const QUuid& uuid, const QVector<QVariant>& data);

    // #### these eventually should be in a base class
    // Requests that the backend set the data for a given UUID.
    void set(const QUuid& uuid, const QByteArray& role, const QVariant& data);

    // #### these eventually should be in a base class
    // Requests that the backend remove a given UUID.
    void remove(const QUuid& uuid);

    void write(const QByteArray& data);

signals:
    void nameChanged();
    void argsChanged();

private slots:
    void handleModelDataReady();

private:
    QString m_name;
    QStringList m_args;
    bool m_completed = false;
    QProcess m_process;
};

