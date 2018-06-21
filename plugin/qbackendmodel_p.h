#pragma once

#include "qbackendobject_p.h"
#include "qbackendmodel.h"
#include <QVariant>
#include <QVector>

class QBackendObject;

class BackendModelPrivate : public BackendObjectPrivate
{
    Q_OBJECT

public:
    using BackendObjectPrivate::BackendObjectPrivate;

    QBackendObject *m_modelData = nullptr;
    QStringList m_roleNames;
    QVector<QVariantList> m_rowData;

    QBackendModel *model() { return static_cast<QBackendModel*>(m_object); }
    void ensureModel();

public slots:
    void doReset(const QVariant &data);
    void doInsert(int start, const QVariant &data);
};
