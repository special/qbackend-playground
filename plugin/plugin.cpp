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
        qmlRegisterSingletonType<QBackendObject>(uri, 1, 0, "Backend",
            [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject*
            {
                Q_UNUSED(scriptEngine);
                QBackendConnection *c = new QBackendConnection(engine);
                // This will force the connection autoconfig and synchronous initialization.
                // If that is successful, it should return the root QBackendObject with its
                // dynamic metaobject.
                //
                // This is a little sketchy, if QML relies on the type's staticMetaObject
                // too much, but we'll find out..
                QObject *root = c->rootObject();
                // The rootObject has JS ownership, and we'll be returning a reference to
                // it from this function. Even though it seems backwards, the easiest way
                // to make sure the connection is destroyed at the proper moment is to make
                // the root object its parent.
                c->setParent(root);
                return root;
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

