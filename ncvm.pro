QT -= core gui

QMAKE_CFLAGS    += -std=c99
QMAKE_CC    = musl-gcc

TARGET = ncvm
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    src/main.c \
    src/std-words.c \
    src/stream.c \
    src/forth-lang.c \
    src/ncvm.c \
    src/lf-queue.c

DISTFILES += \
    LICENSE \
    bootstrap.ncvm

HEADERS += \
    src/internals.h \
    src/module.h
