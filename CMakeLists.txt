cmake_minimum_required(VERSION 3.4)

project(kangzixin LANGUAGES C CXX)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

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


set(kzx_sources
    # src/main.c
    # src/sys/struct/xline.c
    src/ex/malloc.c
    src/ex/pipe.c 
    src/ex/file.c
    src/ex/logger.c
    src/ex/task.c
    # src/env/linetask.c
    # test/env/storage_test.c
    # test/env/logger_test.c
    # test/env/socket_test.c
    # test/sys/struct/heap_test.c
    # test/sys/struct/linedb_test.c
    # test/sys/struct/linekv_test.c
    # test/sys/struct/tree_test.c
    # test/env/linetask_test.c
    # test/test.c
)

set(kzx_defines "")
set(kzx_cflags "")
set(kzx_cxxflags "")
set(kzx_ldflags "")
set(kzx_inclides "")
set(kzx_directories "")

set(kzx_cflags "-m64;-O0;-g;-Wno-format;")

# set(CMAKE_CXX_FLAGS "-Wl,--export-dynamic")

if(CMAKE_CXX_COMPILER_ID STREQUAL Clang OR CMAKE_CXX_COMPILER_ID STREQUAL AppleClang)
    list(APPEND kzx_cflags "-Wno-c++11-narrowing;")
    list(APPEND kzx_ldflags "-Wl,-export_dynamic;")
else()
    list(APPEND kzx_ldflags "-Wl,--export-dynamic;")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND kzx_ldflags pthread dl)
endif()

if(ENV_CHECK_BACKTRACE)
    if (NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        list(APPEND kzx_defines -DENV_MALLOC_BACKTRACE)
        list(APPEND kzx_defines -DENV_MALLOC_PAGE_SIZE=0xA000000 -DENV_MALLOC_MAX_POOL=8)
    endif()
    list(APPEND kzx_defines -DENV_HAVA_BACKTRACE)
    list(APPEND kzx_sources src/ex/backtrace.c)
endif()


function(build_executable EXECUTABLE_NAME EXECUTABLE_SOURCES)
    add_executable(${EXECUTABLE_NAME} ${EXECUTABLE_SOURCES} ${kzx_sources})
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ${kzx_defines})
    target_compile_options(${EXECUTABLE_NAME} PRIVATE ${kzx_cflags})
    target_include_directories(${EXECUTABLE_NAME} PRIVATE src ${kzx_inclides})
    target_link_directories(${EXECUTABLE_NAME} PRIVATE ${kzx_directories})
    target_link_libraries(${EXECUTABLE_NAME} ${kzx_ldflags})
endfunction(build_executable)

# build_executable(kapp src/main.cpp)
build_executable(server src/app/server.c)
# build_executable(client src/app/client.cpp)
# build_executable(kapp test/sys/struct/tree_test.c)
# build_executable(kapp test/sys/struct/heap_test.c)