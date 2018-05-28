TEMPLATE = lib
CONFIG += plugin
QT += qml quick
TARGET = qbackend

IMPORT_PATH = qbackend
target.path = $$[QT_INSTALL_QML]/$$IMPORT_PATH
INSTALLS += target

qmldir.files = qmldir
qmldir.path = $$target.path
INSTALLS += qmldir

SOURCES += \
    plugin.cpp \
    qbackendconnection.cpp \
    qbackendprocess.cpp \
    qbackendjsonlistmodel.cpp \
    qbackendabstractconnection.cpp \
    qbackendstore.cpp \
    qbackendmodel.cpp

HEADERS += \
    plugin.h \
    qbackendconnection.h \
    qbackendprocess.h \
    qbackendjsonlistmodel.h \
    qbackendabstractconnection.h \
    qbackendstore.h \
    qbackendmodel.h
