TEMPLATE = lib
TARGET = qbackend
TARGETPATH = Crimson/QBackend
IMPORT_VERSION = 1.0

QT += qml quick core-private

qmldirConnection.files = Connection/qmldir
qmldirConnection.path = $$[QT_INSTALL_QML]/$$TARGETPATH/Connection/
INSTALLS += qmldirConnection

SOURCES += \
    plugin.cpp \
    qbackendconnection.cpp \
    qbackendprocess.cpp \
    qbackendobject.cpp \
    qbackendmodel.cpp \
    promise.cpp

HEADERS += \
    plugin.h \
    qbackendconnection.h \
    qbackendprocess.h \
    qbackendobject.h \
    qbackendobject_p.h \
    qbackendmodel.h \
    qbackendmodel_p.h \
    instantiable.h \
    promise.h

load(qml_plugin)
