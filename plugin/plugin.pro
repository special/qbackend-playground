TEMPLATE = lib
CONFIG += plugin
QT += qml quick core-private
TARGET = qbackend

IMPORT_PATH = QBackend
target.path = $$[QT_INSTALL_QML]/$$IMPORT_PATH
INSTALLS += target

qmldir.files = qmldir
qmldir.path = $$target.path
INSTALLS += qmldir

qmldirConnection.files = Connection/qmldir
qmldirConnection.path = $$target.path/Connection/
INSTALLS += qmldirConnection

SOURCES += \
    plugin.cpp \
    qbackendconnection.cpp \
    qbackendprocess.cpp \
    qbackendobject.cpp \
    qbackendmodel.cpp

HEADERS += \
    plugin.h \
    qbackendconnection.h \
    qbackendprocess.h \
    qbackendobject.h \
    qbackendobject_p.h \
    qbackendmodel.h \
    qbackendmodel_p.h \
    instantiable.h
