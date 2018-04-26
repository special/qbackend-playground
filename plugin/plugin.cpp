#include "plugin.h"
#include <QQmlEngine>

#include "qbackendprocess.h"
#include "qbackendjsonlistmodel.h"
#include "qbackendstore.h"

void QBackendPlugin::registerTypes(const char *uri)
{
    qmlRegisterType<QBackendProcess>(uri, 1, 0, "BackendProcess");
    qmlRegisterType<QBackendJsonListModel>(uri, 1, 0, "BackendJsonListModel");
    qmlRegisterType<QBackendStore>(uri, 1, 0, "BackendStore");
}

