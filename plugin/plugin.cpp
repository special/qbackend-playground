#include "plugin.h"
#include <QQmlEngine>

#include "qbackendconnection.h"
#include "qbackendprocess.h"
#include "qbackendjsonlistmodel.h"
#include "qbackendobject.h"
#include "qbackendmodel.h"

void QBackendPlugin::registerTypes(const char *uri)
{
    qmlRegisterType<QBackendConnection>(uri, 1, 0, "BackendConnection");
    qmlRegisterType<QBackendProcess>(uri, 1, 0, "BackendProcess");
    qmlRegisterType<QBackendJsonListModel>(uri, 1, 0, "BackendJsonListModel");
    qRegisterMetaType<QBackendObject*>();
    qRegisterMetaType<QBackendModel*>();
}

