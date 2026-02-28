# FanZhou RPC Client Application
# Qt5.12 GUI Client for greenhouse control system
# Target: Ubuntu Desktop

QT += core gui network widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

INCLUDEPATH += src

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
    src/screen_manager.cpp \
    src/views/dashboard_page.cpp \
    src/views/device_control_page.cpp \
    src/views/scene_page.cpp \
    src/views/alarm_page.cpp \
    src/views/sensor_page.cpp \
    src/views/settings_page.cpp

HEADERS += \
    src/mainwindow.h \
    src/rpc_client.h \
    src/screen_manager.h \
    src/style_constants.h \
    src/models/data_models.h \
    src/views/dashboard_page.h \
    src/views/device_control_page.h \
    src/views/scene_page.h \
    src/views/alarm_page.h \
    src/views/sensor_page.h \
    src/views/settings_page.h

# Resources
RESOURCES += \
    resources/resources.qrc

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
