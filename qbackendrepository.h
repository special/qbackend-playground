#pragma once

#include <QObject>

class QBackendModel;

// The repository is populated by backend instances over RPC (e.g.
// QBackendProcess, but maybe other things too).

class QBackendRepository
{
public:
    static QBackendModel *model(const QString& identifier);

private:
    friend class QBackendProcess;
    static void setupModel(const QString& identifier, const QVector<QByteArray>& roleNames);

    static QHash<QString, QBackendModel*> m_models;
};

