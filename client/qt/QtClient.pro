TEMPLATE = app
TARGET = SmartHomeClient

# 客户端按照需求文档使用 Qt 5.14.2，并保持 C++11 兼容性。
QT += core gui network widgets
CONFIG += c++11
CONFIG -= app_bundle

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    ProtocolClient.cpp \
    VlcPlayerWidget.cpp \
    ../../src/Protocol.cc \
    ../../src/UserProtocol.cc \
    ../../src/CameraProtocol.cc

HEADERS += \
    MainWindow.hpp \
    ProtocolClient.hpp \
    VlcPlayerWidget.hpp \
    ../../include/Protocol.hpp \
    ../../include/UserProtocol.hpp \
    ../../include/CameraProtocol.hpp \
    ../../include/CameraStore.hpp

INCLUDEPATH += \
    $$PWD \
    $$PWD/../../include

# 将生成文件集中到客户端目录，便于直接从 Qt Creator 运行和部署。
DESTDIR = $$PWD/bin
OBJECTS_DIR = $$PWD/.qmake/obj
MOC_DIR = $$PWD/.qmake/moc
RCC_DIR = $$PWD/.qmake/rcc
UI_DIR = $$PWD/.qmake/ui

win32:CONFIG += windows
win32:RC_ICONS =
