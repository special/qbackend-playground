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
 *     "modelReset": [ "array rowData" ],
 *     "modelInsert": [ "int start", "array rowData" ],
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

    m_modelData = m_object->property("_qb_model").value<QObject*>();
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

    connect(m_modelData, SIGNAL(modelReset(QJSValue)), this, SLOT(doReset(QJSValue)));
    connect(m_modelData, SIGNAL(modelInsert(int,QJSValue)), this, SLOT(doInsert(int,QJSValue)));
    connect(m_modelData, SIGNAL(modelRemove(int,int)), this, SLOT(doRemove(int,int)));
    connect(m_modelData, SIGNAL(modelMove(int,int,int)), this, SLOT(doMove(int,int,int)));
    connect(m_modelData, SIGNAL(modelUpdate(int,QJSValue)), this, SLOT(doUpdate(int,QJSValue)));

    QMetaObject::invokeMethod(m_modelData, "reset");
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
    if (index.row() < 0 || index.row() >= d->m_rowData.size() || role < Qt::UserRole)
        return QVariant();

    const QJSValue &row = d->m_rowData[index.row()];
    // Note that this is a variant containing a QJSValue, not a QJSValue converted
    // to a variant. Hopefully this is more efficient for QML to deal with.
    return QVariant::fromValue(row.property(role - Qt::UserRole));
}

void BackendModelPrivate::doReset(const QJSValue &data)
{
    model()->beginResetModel();
    m_rowData.clear();

    int size = data.property("length").toNumber();
    m_rowData.reserve(size);
    for (int i = 0; i < size; i++) {
        QJSValue rowData = data.property(i);
        if (!rowData.isArray()) {
            qCWarning(lcModel) << "Model row" << i << "data is not an array";
            continue;
        }
        m_rowData.append(rowData);
    }

    model()->endResetModel();
}

void BackendModelPrivate::doInsert(int start, const QJSValue &data)
{
    int size = data.property("length").toNumber();
    if (size < 1)
        return;

    model()->beginInsertRows(QModelIndex(), start, start + size - 1);
    m_rowData.insert(start, size, QJSValue());
    for (int i = 0; i < size; i++) {
        m_rowData[start+i] = data.property(i);
    }
    model()->endInsertRows();
}

void BackendModelPrivate::doRemove(int start, int end)
{
    model()->beginRemoveRows(QModelIndex(), start, end);
    m_rowData.remove(start, end - start + 1);
    model()->endRemoveRows();
}

void BackendModelPrivate::doMove(int start, int end, int destination)
{
    model()->beginMoveRows(QModelIndex(), start, end, QModelIndex(), destination);
    QVector<QJSValue> rows(end - start + 1);
    std::copy_n(m_rowData.begin()+start, rows.size(), rows.begin());
    // XXX These indicies are likely wrong for the "move down" case that
    // is described in beginMoveRows' documentation.
    m_rowData.remove(start, rows.size());
    m_rowData.insert(destination, rows.size(), QJSValue());
    std::copy_n(rows.begin(), rows.size(), m_rowData.begin()+destination);
    model()->endMoveRows();

}

void BackendModelPrivate::doUpdate(int row, const QJSValue &data)
{
    if (row < 0 || row >= m_rowData.size()) {
        qCWarning(lcModel) << "invalid row" << row << "in model update";
        return;
    }

    m_rowData[row] = data;
    emit model()->dataChanged(model()->index(row), model()->index(row));
}
