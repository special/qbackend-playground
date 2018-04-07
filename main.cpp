#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickView>

#include "qbackendprocess.h"
#include "qbackendlistmodel.h"
#include "qbackendstore.h"

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    qmlRegisterType<QBackendProcess>("com.me", 1, 0, "BackendProcess");
    qmlRegisterType<QBackendListModel>("com.me", 1, 0, "BackendListModel");
    qmlRegisterType<QBackendStore>("com.me", 1, 0, "BackendStore");
    QQuickView view(QUrl::fromLocalFile("main.qml"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.show();
    return app.exec();
}

