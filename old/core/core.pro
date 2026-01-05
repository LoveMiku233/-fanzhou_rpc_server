QT += core network
CONFIG += qt c++11 console
CONFIG -= app_bundle

TEMPLATE = app
TARGET = core_server

INCLUDEPATH += $$PWD/..

SOURCES += \
    ../utils/utils.cpp\
    ../utils/logger.cpp\
    ../config/system_settings.cpp \
    ../comm/base/comm_adapter.cpp \
    ../comm/comm_serial.cpp \
    ../comm/comm_can.cpp \
    ../device/base/device_adapter.cpp \
    ../device/can/device_relay_can_f427.cpp \
    ../device/can/can_device_manager.cpp \
    ../rpc/json_rpc_dispatcher.cpp \
    ../rpc/json_rpc_server.cpp \
    ../rpc/rpc_helpers.cpp \
    ../rpc/rpc_registry.cpp \
    core_config.cpp \
    core_context.cpp \
    core_main.cpp

HEADERS += \
    ../utils/utils.h\
    ../utils/logger.h\
    ../config/system_settings.h \
    ../comm/base/comm_adapter.h \
    ../comm/comm_serial.h \
    ../comm/comm_can.h \
    ../device/device_list.h \
    ../device/base/device_adapter.h \
    ../device/can/i_can_device.h \
    ../device/can/relay_can_protocol.h \
    ../device/can/device_relay_can_f427.h \
    ../device/can/can_device_manager.h \
    ../rpc/rpc_error_codes.h \
    ../rpc/json_rpc_dispatcher.h \
    ../rpc/json_rpc_server.h \
    ../rpc/rpc_registry.h \
    ../rpc/rpc_helpers.h \
    core_config.h \
    core_context.h\
