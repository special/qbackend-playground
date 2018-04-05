#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QUuid>
#include <QProcess>

class QBackendModel;

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
    void invokeMethod(const QString& identifier, const QString& method, const QByteArray& jsonData);
    void invokeMethodOnObject(const QString& identifier, const QUuid& uuid, const QString& method, const QByteArray& jsonData);
    void write(const QByteArray& data);
    QBackendModel* fetchModel(const QString& identifier);

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
    QHash<QString, QBackendModel*> m_models;
};

