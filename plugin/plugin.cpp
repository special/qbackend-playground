#include "plugin.h"
#include <QQmlEngine>
#include <QCoreApplication>

#include "qbackendconnection.h"
#include "qbackendprocess.h"
#include "qbackendobject.h"
#include "qbackendmodel.h"

static QBackendConnection *singleConnection = nullptr;

void QBackendPlugin::registerTypes(const char *uri)
{
    qRegisterMetaType<QBackendObject*>();
    qRegisterMetaType<QBackendModel*>();

    if (QByteArray(uri) == "QBackend") {
        // Make the connection immediately, so it will have an opportunity to register
        // types dynamically. It cannot complete the root object until a QQmlEngine is
        // available, which happens from the singleton callback.
        Q_ASSERT(!singleConnection);
        singleConnection = new QBackendConnection;
        singleConnection->moveToThread(QCoreApplication::instance()->thread());

        qmlRegisterSingletonType<QBackendObject>(uri, 1, 0, "Backend",
            [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject*
            {
                Q_UNUSED(scriptEngine);
                singleConnection->setQmlEngine(engine);
                // This will force the connection autoconfig and synchronous initialization.
                // If that is successful, it should return the root QBackendObject with its
                // dynamic metaobject.
                //
                // This is a little sketchy, if QML relies on the type's staticMetaObject
                // too much, but we'll find out..
                QObject *root = singleConnection->rootObject();
                // The rootObject has JS ownership, and we'll be returning a reference to
                // it from this function. Even though it seems backwards, the easiest way
                // to make sure the connection is destroyed at the proper moment is to make
                // the root object its parent.
                singleConnection->setParent(root);
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

