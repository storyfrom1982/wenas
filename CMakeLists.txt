cmake_minimum_required(VERSION 3.16)

project(kangzixin LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

include (TestBigEndian)
include (CheckCSourceCompiles)
include (CheckFunctionExists)
include (CheckIncludeFiles)
include (CheckTypeSize)

# Check if the system is 64-bit
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "Detected 64-bit system")
else()
    message(FATAL_ERROR "Error: This project requires a 64-bit system. Exiting.")
endif()

TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    message(STATUS "BIG_ENDIAN")
    add_definitions(-D__BIG_ENDIAN__)
else()
    message(STATUS "LITTLE_ENDIAN")
    add_definitions(-D__LITTLE_ENDIAN__)
endif()

check_type_size ("long" ENV_LONG)
if (${ENV_LONG} EQUAL 4)
    message(STATUS "ENV_LONG EQUAL 4")
    add_definitions(-D__ENV_LONG_32__)
elseif (${ENV_LONG} EQUAL 8)
    message(STATUS "ENV_LONG EQUAL 8")
    add_definitions(-D__ENV_LONG_64__)
endif()

check_type_size ("int" ENV_INT)
if (${ENV_INT} EQUAL 2)
    message(STATUS "ENV_INT EQUAL 2")
    add_definitions(-D__ENV_INT_16__)
elseif (${ENV_INT} EQUAL 4)
    message(STATUS "ENV_INT EQUAL 4")
    add_definitions(-D__ENV_INT_32__)
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
    src/xapi/xlog.c
    src/xapi/xapi.c
    src/xnet/xmalloc.c
    src/xnet/xmsger.c
    src/xnet/xtable.c
    src/xlib/avlmini.c
    src/xlib/xxhash.c
    src/xlib/xsha256.c
    src/xnet/uuid.c
    src/xnet/xltp.c
    src/xnet/xlio.c
    # src/app/xpeer.c
)

set(kzx_defines "")
set(kzx_cflags "-Wno-format;")
set(kzx_cxxflags "")
set(kzx_ldflags "")
set(kzx_inclides "")
set(kzx_directories "")

# 检查编译器类型
if(CMAKE_C_COMPILER_ID MATCHES "GNU")
    message(STATUS "Using GCC compiler")
    list(APPEND kzx_defines -DXMALLOC_ENABLE)
    list(APPEND kzx_defines -DXMALLOC_PAGE_SIZE=0x1000000 -DXMALLOC_MAX_POOL=8)
    list(APPEND kzx_ldflags "-Wl,--export-dynamic;")
endif()

if(CMAKE_C_COMPILER_ID STREQUAL Clang OR CMAKE_C_COMPILER_ID STREQUAL AppleClang)
    message(STATUS "Using Clang compiler")
    if(NOT DEFINED ENV{QTDIR})
        list(APPEND kzx_ldflags "-Wl,-export_dynamic;")
        list(APPEND kzx_defines -DXMALLOC_ENABLE)
        list(APPEND kzx_defines -DXMALLOC_PAGE_SIZE=0x1000000 -DXMALLOC_MAX_POOL=8)
    endif()
endif()

if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
    message(STATUS "Detected Android system")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND kzx_ldflags pthread dl)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND kzx_cflags "-m64;-O0;-g;")
    list(APPEND kzx_defines -D__XDEBUG__)
endif()

if(ENV_CHECK_BACKTRACE)
    list(APPEND kzx_defines -DUNWIND_BACKTRACE)
    list(APPEND kzx_sources src/xapi/backtrace.c)
endif()

list(APPEND kzx_inclides src/xapi/dep src/xapi/dep/libuv-1.49.2/include)

add_subdirectory(src/xapi/dep/libuv-1.49.2)
# list(APPEND kzx_directories src/xapi/dep)

function(build_executable EXECUTABLE_NAME EXECUTABLE_SOURCES)
    add_executable(${EXECUTABLE_NAME} ${EXECUTABLE_SOURCES} ${kzx_sources})
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ${kzx_defines})
    target_compile_options(${EXECUTABLE_NAME} PRIVATE ${kzx_cflags})
    target_include_directories(${EXECUTABLE_NAME} PRIVATE src ${kzx_inclides})
    target_link_directories(${EXECUTABLE_NAME} PRIVATE ${kzx_directories})
    target_link_libraries(${EXECUTABLE_NAME} ${kzx_ldflags} uv)
endfunction(build_executable)

function(build_library LIBRARY_NAME LIBRARY_SOURCES)
    add_library(${LIBRARY_NAME} ${LIBRARY_SOURCES} ${kzx_sources})
    target_compile_definitions(${LIBRARY_NAME} PRIVATE ${kzx_defines})
    target_compile_options(${LIBRARY_NAME} PRIVATE ${kzx_cflags})
    target_include_directories(${LIBRARY_NAME} PRIVATE src ${kzx_inclides})
    target_link_directories(${LIBRARY_NAME} PRIVATE ${kzx_directories})
    target_link_libraries(${LIBRARY_NAME} ${kzx_ldflags} uv)
endfunction(build_library)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    build_executable(xltpd src/app/xltpd.c)
    build_executable(xpeer src/main.c)
    # build_executable(xserver test/xnet/dht_test.c)
    # build_executable(xserver test/xnet/xline_test.c)
    # build_executable(xchord src/app/xchord.c)
else()
    build_library(xuser src/app/xpeer.c)
endif()
