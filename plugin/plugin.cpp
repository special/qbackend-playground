#include "plugin.h"
#include <QQmlEngine>

#include "qbackendconnection.h"
#include "qbackendprocess.h"
#include "qbackendobject.h"
#include "qbackendmodel.h"

void QBackendPlugin::registerTypes(const char *uri)
{
    qRegisterMetaType<QBackendObject*>();
    qRegisterMetaType<QBackendModel*>();

    if (QByteArray(uri) == "QBackend") {
        // The QBackend import registers a singleton connection, which is configured
        // through properties of the root context, commandline arguments, or environment.
        qmlRegisterSingletonType<QBackendConnection>(uri, 1, 0, "Backend",
            [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject*
            {
                Q_UNUSED(engine);
                Q_UNUSED(scriptEngine);
                return new QBackendConnection(engine);
            }
        );
    } else if (QByteArray(uri) == "QBackend.Connection") {
        // QBackend.Connection exposes explicit types for the connection, including a
        // type to execute a new process for the backend.
        qmlRegisterType<QBackendConnection>(uri, 1, 0, "BackendConnection");
        qmlRegisterType<QBackendProcess>(uri, 1, 0, "BackendProcess");
    } else {
        Q_ASSERT_X(false, "QBackendPlugin", "unexpected plugin URI");
    }
}

