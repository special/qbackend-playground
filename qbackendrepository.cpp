#include <QDebug>

#include "qbackendrepository.h"
#include "qbackendmodel.h"

QHash<QString, QBackendModel*> QBackendRepository::m_models;

QBackendModel *QBackendRepository::model(const QString& identifier)
{
    Q_ASSERT(m_models.contains(identifier));
    return m_models[identifier];
}

void QBackendRepository::setupModel(QBackendProcess* connection, const QString& identifier, const QVector<QByteArray>& roleNames)
{
    qWarning() << "Got data for model " << identifier << " role names " << roleNames << " living on connection " << connection;
    Q_ASSERT(!m_models.contains(identifier));
    m_models[identifier] = new QBackendModel(connection, identifier, roleNames);
}

