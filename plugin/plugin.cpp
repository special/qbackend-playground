#include "plugin.h"
#include <QQmlEngine>

#include "qbackendconnection.h"
#include "qbackendprocess.h"
#include "qbackendobject.h"
#include "qbackendmodel.h"

void QBackendPlugin::registerTypes(const char *uri)
{
    qmlRegisterType<QBackendConnection>(uri, 1, 0, "BackendConnection");
    qmlRegisterType<QBackendProcess>(uri, 1, 0, "BackendProcess");
    qRegisterMetaType<QBackendObject*>();
    qRegisterMetaType<QBackendModel*>();
}

