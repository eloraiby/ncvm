cmake_minimum_required(VERSION 3.1)
project(ncvm)

include_directories(include src)

FILE(GLOB_RECURSE HeaderFiles "*.h")
add_custom_target(headers SOURCES ${HeaderFiles})

FILE(GLOB_RECURSE NcvmFiles "*.ncvm")
add_custom_target(scripts SOURCES ${NcvmFiles})

find_package(Threads REQUIRED)

# nano combinator VM
add_executable(ncvm src/lock-free/uqueue.c
                    src/lock-free/bqueue.c
                    src/main.c
                    src/ncvm.c
                    src/std-words.c
                    src/stream.c)
target_link_libraries(ncvm "${CMAKE_THREAD_LIBS_INIT}")

set_property(TARGET ncvm PROPERTY C_STANDARD 11)

################################################################################
# Tests
################################################################################

# unbounded queue test
add_executable(test_uqueue  test/lock-free/uqueue.c
                            src/lock-free/uqueue.c)

target_link_libraries(test_uqueue "${CMAKE_THREAD_LIBS_INIT}")
set_property(TARGET test_uqueue PROPERTY C_STANDARD 11)

# nbounded queue test
add_executable(test_bqueue  test/lock-free/bqueue.c
                            src/lock-free/bqueue.c)

target_link_libraries(test_bqueue "${CMAKE_THREAD_LIBS_INIT}")
set_property(TARGET test_bqueue PROPERTY C_STANDARD 11)
