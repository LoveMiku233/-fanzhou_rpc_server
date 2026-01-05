# FanZhou RPC Server
# Qt project file for greenhouse control system core service

QT += core network
QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

TEMPLATE = app
TARGET = fanzhou_rpc_server

# Source directory
INCLUDEPATH += $$PWD

SOURCES += \
    main.cpp \
    src/utils/utils.cpp \
    src/utils/logger.cpp \
    src/config/system_settings.cpp \
    src/comm/base/comm_adapter.cpp \
    src/comm/serial_comm.cpp \
    src/comm/can_comm.cpp \
    src/device/base/device_adapter.cpp \
    src/device/can/can_device_manager.cpp \
    src/device/can/relay_gd427.cpp \
    src/rpc/rpc_helpers.cpp \
    src/rpc/json_rpc_dispatcher.cpp \
    src/rpc/json_rpc_server.cpp \
    src/rpc/json_rpc_client.cpp \
    src/core/core_config.cpp \
    src/core/core_context.cpp \
    src/core/rpc_registry.cpp

HEADERS += \
    src/utils/utils.h \
    src/utils/logger.h \
    src/config/system_settings.h \
    src/comm/base/comm_adapter.h \
    src/comm/serial_comm.h \
    src/comm/can_comm.h \
    src/device/device_types.h \
    src/device/base/device_adapter.h \
    src/device/can/i_can_device.h \
    src/device/can/relay_protocol.h \
    src/device/can/can_device_manager.h \
    src/device/can/relay_gd427.h \
    src/rpc/rpc_error_codes.h \
    src/rpc/rpc_helpers.h \
    src/rpc/json_rpc_dispatcher.h \
    src/rpc/json_rpc_server.h \
    src/rpc/json_rpc_client.h \
    src/core/core_config.h \
    src/core/core_context.h \
    src/core/rpc_registry.h

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
