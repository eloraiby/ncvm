QT -= core gui

QMAKE_CFLAGS    += -std=c99

TARGET = nanoforth
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    main.c

DISTFILES += \
    LICENSE
