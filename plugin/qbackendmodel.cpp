#include "qbackendmodel.h"
#include "qbackendobject.h"
#include "qbackendmodel_p.h"
#include <QQmlEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcModel, "backend.model")

/* Models are QBackendObjects with QAbstractListModel behavior client-side, using an internal object API.
 * A QBackendModel is a fully functional object and can have user-defined properties, methods, and signals.
 *
 * Objects with a '_qb_model' property of type 'object' will construct a QBackendModel. See below for
 * details on this object.
 */

QBackendModel::QBackendModel(QBackendAbstractConnection *connection, QByteArray identifier, const QJsonObject &type, QObject *parent)
    : QAbstractListModel(parent)
    , d(new BackendModelPrivate(this, connection, identifier))
    , m_metaObject(d->metaObjectFromType(type, &QAbstractListModel::staticMetaObject))
{
}

QBackendModel::~QBackendModel()
{
    d->deleteLater();
    free(m_metaObject);
}

const QMetaObject *QBackendModel::metaObject() const
{
    Q_ASSERT(m_metaObject);
    return m_metaObject;
}

int QBackendModel::qt_metacall(QMetaObject::Call c, int id, void **argv)
{
    id = QAbstractListModel::qt_metacall(c, id, argv);
    if (id < 0)
        return id;
    return d->metacall(c, id, argv);
}

/* The _qb_model object must implement:
 *
 * {
 *   "properties": {
 *     "roleNames": "var" // string list
 *   },
 *   "methods": {
 *     "reset": []
 *   },
 *   "signals": {
 *     "modelReset": [ "var rowData" ],
 *     "modelInsert": [ "int start", "var rowData" ],
 *     "modelRemove": [ "int start", "int end" ],
 *     "modelMove": [ "int start", "int end", "int destination" ],
 *     "modelUpdate": [ "int row", "var rowData" ]
 *   }
 * }
 *
 */

void BackendModelPrivate::ensureModel()
{
    if (m_modelData)
        return;

    m_modelData = m_object->property("_qb_model").value<QBackendObject*>();
    if (!m_modelData) {
        qCWarning(lcModel) << "Missing _qb_model object on model type" << m_object->metaObject()->className();
        return;
    }
    m_modelData->setParent(this);

    m_roleNames = m_modelData->property("roleNames").value<QStringList>();
    if (m_roleNames.isEmpty()) {
        qCWarning(lcModel) << "Model type" << m_object->metaObject()->className() << "has no role names";
        return;
    }

    connect(m_modelData, SIGNAL(modelReset(QVariant)), this, SLOT(doReset(QVariant)));
    connect(m_modelData, SIGNAL(modelInsert(int,QVariant)), this, SLOT(doInsert(int,QVariant)));

    QMetaObject::invokeMethod(m_modelData, "Reset");

    // XXX what if _qb_model changes -- full reset, or panic and error
    // XXX how is data represented
    // XXX how is data handled w/o a change signal and sending the whole thing every time, given object API
    //
    // One option is to not put model data in properties; instead, it could be sent in via signals. reset with
    // all, etc. Yes, that seems reasonable.
}

QHash<int, QByteArray> QBackendModel::roleNames() const
{
    const_cast<QBackendModel*>(this)->d->ensureModel();
    QHash<int,QByteArray> roles;
    for (const QString &name : d->m_roleNames)
        roles[Qt::UserRole + roles.size()] = name.toUtf8();
    return roles;
}

int QBackendModel::rowCount(const QModelIndex&) const
{
    const_cast<QBackendModel*>(this)->d->ensureModel();
    return d->m_rowData.size();
}

QVariant QBackendModel::data(const QModelIndex &index, int role) const
{
    const_cast<QBackendModel*>(this)->d->ensureModel();
    if (index.row() < 0 || index.row() >= d->m_rowData.size())
        return QVariant();

    const QVariantList &row = d->m_rowData[index.row()];
    if (role < Qt::UserRole || (role - Qt::UserRole) > row.size())
        return QVariant();

    return row[role - Qt::UserRole];
}

void BackendModelPrivate::doReset(const QVariant &data)
{
    model()->beginResetModel();
    m_rowData.clear();

    QVariantList rows = data.toList();
    m_rowData.reserve(rows.size());
    for (const QVariant &row : rows) {
        QVariantList rowData = row.toList();
        if (rowData.size() != m_roleNames.size()) {
            qCWarning(lcModel) << "Model row" << m_rowData.size() << "has" << rowData.size() << "fields but model expects" << m_roleNames.size();
        }
        m_rowData.append(rowData);
    }

    model()->endResetModel();
}

void BackendModelPrivate::doInsert(int start, const QVariant &data)
{
    // TODO
}

#if 0
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
#endif

#if 0
    QJsonObject args = params.toObject();

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
#endif
