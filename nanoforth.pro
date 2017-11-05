QT -= core gui

QMAKE_CFLAGS    += -std=c99
QMAKE_CC    = musl-gcc

TARGET = nanoforth
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    main.c \
    std-words.c \
    vm.c \
    stream.c \
    forth-lang.c

DISTFILES += \
    LICENSE \
    bootstrap.4th

HEADERS += \
    nanoforth.h
