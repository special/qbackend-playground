#include "qbackendmodel.h"
#include <QQmlEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcModel, "backend.model")

QHash<std::pair<QBackendAbstractConnection*,QString>, std::weak_ptr<QBackendInternalModel>> QBackendInternalModel::m_instances;

class QBackendModelProxy : public QBackendRemoteObject
{
public:
    QBackendModelProxy(QBackendInternalModel *model);

    void objectFound(const QJsonDocument& document) override;
    void methodInvoked(const QByteArray& method, const QJsonDocument& document) override;

private:
    QBackendInternalModel *m_model;
};

QBackendModel::QBackendModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

QBackendModel::~QBackendModel()
{
}

QByteArray QBackendModel::identifier() const
{
    return m_identifier;
}

void QBackendModel::setIdentifier(const QByteArray& identifier)
{
    if (m_identifier == identifier)
        return;

    m_identifier = identifier;
    subscribeIfReady();
    emit identifierChanged();
}

QStringList QBackendModel::roles() const
{
    return m_roleNames;
}

void QBackendModel::setRoles(const QStringList &roleNames)
{
    if (m_roleNames == roleNames)
        return;

    m_roleNames = roleNames;
    subscribeIfReady();
    emit roleNamesChanged();
}

QBackendAbstractConnection* QBackendModel::connection() const
{
    return m_connection;
}

void QBackendModel::setConnection(QBackendAbstractConnection* connection)
{
    if (m_connection == connection)
        return;

    m_connection = connection;
    subscribeIfReady();
    emit connectionChanged();
}

void QBackendModel::invokeMethod(const QString& method, const QJSValue& data)
{
    QJSEngine *engine = qmlEngine(this);
    QJSValue global = engine->globalObject();
    QJSValue json = global.property("JSON");
    QJSValue stringify = json.property("stringify");
    QJSValue jsonData = stringify.call(QList<QJSValue>() << data);
    m_connection->invokeMethod(m_identifier, method, jsonData.toString().toUtf8());
}

void QBackendModel::subscribeIfReady()
{
    if (m_model) {
        setSourceModel(nullptr);
        m_model.reset();
    }

    if (!m_connection || m_identifier.isEmpty() || m_roleNames.isEmpty())
        return;

    m_model = QBackendInternalModel::instance(m_connection, m_identifier);
    m_model->setRoleNames(m_roleNames);
    setSourceModel(m_model.get());
}

std::shared_ptr<QBackendInternalModel> QBackendInternalModel::instance(QBackendAbstractConnection *connection, QByteArray identifier)
{
    auto key = std::make_pair(connection, identifier);
    auto model = m_instances.value(key).lock();
    if (!model) {
        model = std::shared_ptr<QBackendInternalModel>(new QBackendInternalModel(connection, identifier));
        m_instances.insert(key, model);
    }
    return model;
}

QBackendInternalModel::QBackendInternalModel(QBackendAbstractConnection *connection, QByteArray identifier)
    : m_identifier(identifier)
    , m_connection(connection)
{
    m_proxy = new QBackendModelProxy(this);
    m_connection->subscribe(m_identifier, m_proxy);
}

QBackendInternalModel::~QBackendInternalModel()
{
    if (m_connection && m_proxy)
        m_connection->unsubscribe(m_identifier, m_proxy);
    if (m_proxy)
        m_proxy->deleteLater();
}

QHash<int, QByteArray> QBackendInternalModel::roleNames() const
{
    return m_roleNames;
}

int QBackendInternalModel::rowCount(const QModelIndex&) const
{
    return m_data.size();
}

QVariant QBackendInternalModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_data.size())
        return QVariant();

    QByteArray roleName = m_roleNames.value(role);
    if (roleName.isEmpty())
        return QVariant();

    return m_data[index.row()][roleName];
}

// XXX This is extremely sensitive to the set of roles used for different instances of the model;
// any difference is likely to lead to brokenness. Should get these from backend instead?
void QBackendInternalModel::setRoleNames(QStringList names)
{
    // Sort names for stable role order between instances of the same model
    names.sort();

    // ### should validate that the backend knows about these roles
    for (const QString& role : names) {
        bool found = false;
        for (const auto &r : m_roleNames) {
            if (r == role) {
                found = true;
                break;
            }
        }

        if (!found)
            m_roleNames[Qt::UserRole + m_roleNames.count()] = role.toUtf8();
    }
}

void QBackendInternalModel::doReset(const QJsonDocument &document)
{
    qCDebug(lcModel) << "Resetting" << m_identifier;
    emit beginResetModel();
    m_data.clear();

    Q_ASSERT(document.isObject());
    if (!document.isObject()) {
        qCWarning(lcModel) << "Got a document not an object: " << document;
        emit endResetModel();
        return;
    }

    QJsonObject dataObject = document.object();
    QJsonObject::const_iterator datait;
    if ((datait = dataObject.constFind("data")) == dataObject.constEnd()) {
        qCDebug(lcModel) << "No data object found";
        emit endResetModel();
        return; // no rows
    }

    Q_ASSERT(datait.value().isArray());
    QJsonArray rows = datait.value().toArray();

    if (rows.size() == 0) {
        qCDebug(lcModel) << "Empty data object";
        emit endResetModel();
        return; // no rows
    }

    m_data.reserve(rows.size());
    for (auto rowit = rows.constBegin(); rowit != rows.constEnd(); rowit++) {
        Q_ASSERT(rowit->isObject());
        QJsonObject row = rowit->toObject();

        QMap<QByteArray, QVariant> objectData;
        for (auto propit = row.constBegin(); propit != row.constEnd(); propit++) {
            objectData[propit.key().toUtf8()] = propit.value().toVariant();
        }

        m_data.append(objectData);
    }

    emit endResetModel();
}

QBackendModelProxy::QBackendModelProxy(QBackendInternalModel *model)
    : m_model(model)
{
}

void QBackendModelProxy::objectFound(const QJsonDocument& document)
{
    m_model->doReset(document);
}

void QBackendModelProxy::methodInvoked(const QByteArray& method, const QJsonDocument& document)
{
    // XXX
    qFatal("not implemented");
}
