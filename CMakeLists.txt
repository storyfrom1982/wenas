cmake_minimum_required(VERSION 3.4)

project(kangzixin LANGUAGES C)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)

include (TestBigEndian)
include (CheckCSourceCompiles)
include (CheckFunctionExists)
include (CheckIncludeFiles)
include (CheckTypeSize)

TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    message(STATUS "BIG_ENDIAN")
    add_definitions(-D__BIG_ENDIAN__)
else()
    message(STATUS "LITTLE_ENDIAN")
    add_definitions(-D__LITTLE_ENDIAN__)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "CMAKE_SIZEOF_VOID_P EQUAL 8")
    add_definitions(-D__PL64__)
endif()

check_type_size ("long" ENV_LONG)
if (${ENV_LONG} EQUAL 4)
    message(STATUS "ENV_LONG EQUAL 4")
    add_definitions(-D__ENV_LONG_32__)
elseif (${ENV_LONG} EQUAL 8)
    message(STATUS "ENV_LONG EQUAL 8")
    add_definitions(-D__ENV_LONG_64__)
endif()

check_c_source_compiles("
    #include <unwind.h>
    #define __USE_GNU
    #include <dlfcn.h>
    int main() {
        struct _Unwind_Context *ctx;
        return 0;
    }" ENV_CHECK_BACKTRACE)

set(kzx_sources src/main.c 
    src/env/time.c
    src/env/storage.c
    src/env/thread.c
    src/env/logger.c
    src/env/backtrace.c
    test/test.c
    test/env/storage_test.c
    test/env/thread_test.c
    test/env/logger_test.c
    # src/env/malloc.c
    # src/env/socket.c
    # test/sys/struct/heap_test.c
    # test/sys/struct/lineardb_test.c
    # test/sys/struct/linearkv_test.c
    # test/sys/struct/lineardb_pipe_test.c
    # test/env/task/task_queue_test.c
    # test/env/malloc/malloc_test.c
)

set(kzx_defines "")
set(kzx_cflags "")
set(kzx_ldflags "")

if(ENV_CHECK_BACKTRACE)
    list(APPEND kzx_defines -DENV_HAVA_BACKTRACE)
endif()

if (UNIX)
    set(kzx_cflags "-Wno-format")
    # set(kzx_cflags "-Wformat")
    list(APPEND kzx_defines -DENV_MALLOC_PAGE_SIZE=0xA00000 -DENV_MALLOC_MAX_POOL=16)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND kzx_ldflags pthread dl)
    endif()

    list(APPEND kzx_sources src/env/unix/unix.c)

else()

    list(APPEND kzx_sources src/env/win/win.c)

endif()

add_executable(kang ${kzx_sources})
target_compile_definitions(kang PRIVATE ${kzx_defines})
target_compile_options(kang PRIVATE ${kzx_cflags})
target_include_directories(kang PRIVATE src)
target_link_libraries(kang ${kzx_ldflags})
