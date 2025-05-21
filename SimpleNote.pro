QT       += core gui sql network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
CONFIG += utf8_source

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# 应用程序图标和版本信息
QMAKE_TARGET_COMPANY = Biubush
QMAKE_TARGET_PRODUCT = SimpleNote
QMAKE_TARGET_DESCRIPTION = "简单便签"
QMAKE_TARGET_COPYRIGHT = "版权所有 (C) 2025 Biubush"

# 确保中文编码正确
CODECFORTR = UTF-8
CODECFORSRC = UTF-8

# Windows资源文件
win32:RC_FILE = SimpleNote.rc
win32:RC_CODEPAGE = 65001

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    note.cpp \
    noteeditwidget.cpp \
    notedatabase.cpp \
    notelistwidget.cpp

HEADERS += \
    mainwindow.h \
    note.h \
    noteeditwidget.h \
    notedatabase.h \
    notelistwidget.h

FORMS += \
    mainwindow.ui \
    noteeditwidget.ui \
    notelistwidget.ui

RESOURCES += \
    resources.qrc

# 添加打包脚本
win32 {
    # 创建打包脚本
    deploy_script.input = $$PWD/deploy.bat.in
    deploy_script.output = $$PWD/deploy.bat
    QMAKE_SUBSTITUTES += deploy_script
    
    # 自定义打包命令
    deploy.commands = $$PWD/deploy.bat
    deploy.depends = $$TARGET
    QMAKE_EXTRA_TARGETS += deploy
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
