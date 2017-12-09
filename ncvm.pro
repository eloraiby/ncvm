QT -= core gui

QMAKE_CFLAGS    += -std=c99
QMAKE_CC    = musl-gcc

TARGET = ncvm
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    main.c \
    std-words.c \
    stream.c \
    forth-lang.c \
    ncvm.c

DISTFILES += \
    LICENSE \
    bootstrap.ncvm

HEADERS += \
    ncvm.h
