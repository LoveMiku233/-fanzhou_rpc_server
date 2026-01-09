# FanZhou RPC Client Application
# Qt5.12 GUI Client for greenhouse control system
# Target: Ubuntu Desktop

QT += core gui network widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

TEMPLATE = app
TARGET = fanzhou_rpc_client

# Define output directories
DESTDIR = $$PWD/bin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
UI_DIR = $$PWD/build/ui
RCC_DIR = $$PWD/build/rcc

# Source files
SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/rpc_client.cpp \
    src/device_widget.cpp \
    src/group_widget.cpp \
    src/connection_dialog.cpp \
    src/connection_widget.cpp \
    src/relay_control_widget.cpp

HEADERS += \
    src/mainwindow.h \
    src/rpc_client.h \
    src/device_widget.h \
    src/group_widget.h \
    src/connection_dialog.h \
    src/connection_widget.h \
    src/relay_control_widget.h

# Resources
RESOURCES += \
    resources/resources.qrc

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
