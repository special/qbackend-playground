#include "promise.h"
#include <QLoggingCategory>
#include <QDebug>

Q_DECLARE_LOGGING_CATEGORY(lcObject);

Promise::Promise(QJSEngine *engine)
{
    // XXX Could QJSValue::callAsConstructor avoid evaluation here?
    QJSValue v = engine->evaluate(" \
        var r = ({}); \
        r['promise'] = new Promise(function(resolve, reject) { \
            r['resolve'] = resolve; \
            r['reject'] = reject; \
        }); \
        r; \
    ");
    if (v.isError()) {
        qCCritical(lcObject) << "Failed to create promise:" << v.toString();
        return;
    }

    m_value = v.property("promise");
    m_resolve = v.property("resolve");
    m_reject = v.property("reject");
    if (!m_value.isObject() || !m_resolve.isCallable() || !m_reject.isCallable()) {
        qCCritical(lcObject) << "Failed to create promise" << m_value.isObject() << m_resolve.isCallable() << m_reject.isCallable();
        return;
    }
}

void Promise::resolve(const QJSValue &result)
{
    Q_ASSERT(m_resolve.isCallable());
    m_resolve.call({result});
    m_resolve = m_reject = QJSValue();
}

void Promise::reject(const QJSValue &error)
{
    Q_ASSERT(m_resolve.isCallable());
    m_reject.call({error});
    m_resolve = m_reject = QJSValue();
}
