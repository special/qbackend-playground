#pragma once

#include <QJSEngine>

class Promise
{
public:
    Promise(QJSEngine *engine);

    QJSValue value() const { return m_value; }
    void resolve(const QJSValue &result);
    void reject(const QJSValue &error);

private:
    QJSValue m_value;
    QJSValue m_resolve;
    QJSValue m_reject;
};
