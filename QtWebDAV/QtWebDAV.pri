INCLUDEPATH += $$PWD
QT += xml

DEFINES += QT_USE_QSTRINGBUILDER
DEFINES += DEBUG_WEBDAV
DEFINES += QTWEBDAV

HEADERS += \
    $$PWD/qnaturalsort.h \
    $$PWD/qwebdav.h \
    $$PWD/qwebdav_global.h \
    $$PWD/qwebdavdirparser.h \
    $$PWD/qwebdavitem.h

SOURCES += \
    $$PWD/qnaturalsort.cpp \
    $$PWD/qwebdav.cpp \
    $$PWD/qwebdavdirparser.cpp \
    $$PWD/qwebdavitem.cpp 