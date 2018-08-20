#pragma once

#include "qbackendobject_p.h"
#include "qbackendmodel.h"
#include <QVariant>
#include <QVector>
#include <QJSValue>

class BackendModelPrivate : public BackendObjectPrivate
{
    Q_OBJECT

public:
    using BackendObjectPrivate::BackendObjectPrivate;

    QObject *m_modelData = nullptr;
    QStringList m_roleNames;
    QMap<int,QJSValue> m_rowData;
    int m_rowCount = 0;
    int m_batchSize = 100;
    int m_cacheSize = 1000;

    QBackendModel *model() { return static_cast<QBackendModel*>(m_object); }
    void ensureModel();
    QJSValue fetchRow(int row);
    void cleanRowCache(int rowHint);

public slots:
    void doReset(const QJSValue &data, int moreRows);
    void doInsert(int start, const QJSValue &data, int moreRows);
    void doRemove(int start, int end);
    void doMove(int start, int end, int destination);
    void doUpdate(int row, const QJSValue &data);
    void doRowData(int row, const QJSValue &data);
};
