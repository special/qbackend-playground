#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickView>

#include "qbackendprocess.h"
#include "qbackendlistmodel.h"

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    qmlRegisterType<QBackendProcess>("com.me", 1, 0, "BackendProcess");
    qmlRegisterType<QBackendListModel>("com.me", 1, 0, "BackendListModel");
    QQuickView view(QUrl::fromLocalFile("main.qml"));
    view.show();
    return app.exec();
}

#include "main.moc"
