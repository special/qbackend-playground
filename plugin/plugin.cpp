#include "plugin.h"
#include <QQmlEngine>

#include "qbackendconnection.h"
#include "qbackendprocess.h"
#include "qbackendobject.h"
#include "qbackendmodel.h"

void QBackendPlugin::registerTypes(const char *uri)
{
    qmlRegisterType<QBackendConnection>(uri, 1, 0, "BackendConnection");
    qmlRegisterSingletonType<QBackendConnection>(uri, 1, 0, "Backend",
        [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject*
        {
            Q_UNUSED(engine);
            Q_UNUSED(scriptEngine);
            return new QBackendConnection(engine);
        }
    );
    qmlRegisterType<QBackendProcess>(uri, 1, 0, "BackendProcess");
    qRegisterMetaType<QBackendObject*>();
    qRegisterMetaType<QBackendModel*>();
}

