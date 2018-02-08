cmake_minimum_required(VERSION 3.1)
project(ncvm)

include_directories(include src)

FILE(GLOB_RECURSE HeaderFiles "*.h")
add_custom_target(headers SOURCES ${HeaderFiles})

FILE(GLOB_RECURSE NcvmFiles "*.ncvm")
add_custom_target(scripts SOURCES ${NcvmFiles})

add_executable(ncvm src/lock-free/uqueue.c
                    src/lock-free/bqueue.c
                    src/lock-free/bstack.c
                    src/main.c
                    src/ncvm.c
                    src/std-words.c
                    src/stream.c)

set_property(TARGET ncvm PROPERTY C_STANDARD 11)
