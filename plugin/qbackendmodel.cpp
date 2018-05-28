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

static QMap<QByteArray, QVariant> rowDataFromJson(const QJsonObject &row)
{
    QMap<QByteArray, QVariant> objectData;
    for (auto propit = row.constBegin(); propit != row.constEnd(); propit++) {
        objectData[propit.key().toUtf8()] = propit.value().toVariant();
    }
    return objectData;
}

void QBackendInternalModel::doReset(const QJsonObject &dataObject)
{
    qCDebug(lcModel) << "Resetting" << m_identifier;
    emit beginResetModel();
    m_data.clear();

    QJsonArray rows = dataObject.value("data").toArray();
    if (rows.size() == 0)
        qCDebug(lcModel) << "Empty data object";

    m_data.reserve(rows.size());
    for (auto rowit = rows.constBegin(); rowit != rows.constEnd(); rowit++) {
        Q_ASSERT(rowit->isObject());
        QJsonObject row = rowit->toObject();
        m_data.append(rowDataFromJson(row));
    }

    emit endResetModel();
}

void QBackendInternalModel::doInsert(int start, const QJsonArray &rows)
{
    beginInsertRows(QModelIndex(), start, start + rows.count() - 1);
    m_data.insert(start, rows.count(), QMap<QByteArray,QVariant>());
    int i = start;
    for (const auto &row : rows) {
        m_data[i] = rowDataFromJson(row.toObject());
        i++;
    }
    endInsertRows();
}

void QBackendInternalModel::doRemove(int start, int end)
{
    beginRemoveRows(QModelIndex(), start, end);
    m_data.remove(start, end - start + 1);
    endRemoveRows();
}

void QBackendInternalModel::doMove(int start, int end, int destination)
{
    beginMoveRows(QModelIndex(), start, end, QModelIndex(), destination);
    QVector<QMap<QByteArray,QVariant>> rows(end - start + 1);
    std::copy_n(m_data.begin()+start, rows.size(), rows.begin());
    // XXX These indicies are likely wrong for the "move down" case that
    // is described in beginMoveRows' documentation.
    m_data.remove(start, rows.size());
    m_data.insert(destination, rows.size(), QMap<QByteArray,QVariant>());
    std::copy_n(rows.begin(), rows.size(), m_data.begin()+destination);
    endMoveRows();

}
void QBackendInternalModel::doUpdate(int row, const QJsonObject &data)
{
    if (row < 0 || row >= m_data.size()) {
        qCWarning(lcModel) << "invalid row" << row << "in model update";
        return;
    }

    m_data[row] = rowDataFromJson(data);
    emit dataChanged(index(row), index(row));
}

QBackendModelProxy::QBackendModelProxy(QBackendInternalModel *model)
    : m_model(model)
{
}

void QBackendModelProxy::objectFound(const QJsonDocument& document)
{
    if (!document.isObject()) {
        qCWarning(lcModel) << "Invalid data type for backend model object";
        return;
    }

    m_model->doReset(document.object());
}

void QBackendModelProxy::methodInvoked(const QByteArray& method, const QJsonDocument& document)
{
    if (!document.isObject()) {
        qCWarning(lcModel) << "Method invoked without valid data";
        return;
    }
    QJsonObject args = document.object();

    if (method == "insert") {
        // { "start": 0, rows: [ { ... } ] }
        int start = args.value("start").toInt();
        QJsonArray rows = args.value("rows").toArray();
        m_model->doInsert(start, rows);
    } else if (method == "remove") {
        // { "start": 0, "end": 0 }
        int start = args.value("start").toInt();
        int end = args.value("end").toInt();
        m_model->doRemove(start, end);
    } else if (method == "move") {
        // { "start": 0, "end": 0, "destination": 0 }
        int start = args.value("start").toInt();
        int end = args.value("end").toInt();
        int destination = args.value("destination").toInt();
        m_model->doMove(start, end, destination);
    } else if (method == "update") {
        // { rows: { "0": { ... } } }
        QJsonObject rowMap = args.value("rows").toObject();
        for (auto it = rowMap.constBegin(); it != rowMap.constEnd(); it++) {
            m_model->doUpdate(it.key().toInt(), it.value().toObject());
        }
    } else if (method == "reset") {
        // identical to data object in objectFound
        m_model->doReset(args);
    } else {
        qCWarning(lcModel) << "unknown method" << method << "invoked";
    }
}
