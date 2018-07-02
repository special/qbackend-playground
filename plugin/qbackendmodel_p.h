#pragma once

#include "qbackendobject_p.h"
#include "qbackendmodel.h"
#include <QVariant>
#include <QVector>
#include <QJSValue>

class QBackendObject;

class BackendModelPrivate : public BackendObjectPrivate
{
    Q_OBJECT

public:
    using BackendObjectPrivate::BackendObjectPrivate;

    QBackendObject *m_modelData = nullptr;
    QStringList m_roleNames;
    QVector<QJSValue> m_rowData;

    QBackendModel *model() { return static_cast<QBackendModel*>(m_object); }
    void ensureModel();

public slots:
    void doReset(const QJSValue &data);
    void doInsert(int start, const QJSValue &data);
    void doRemove(int start, int end);
    void doMove(int start, int end, int destination);
    void doUpdate(int row, const QJSValue &data);
};
