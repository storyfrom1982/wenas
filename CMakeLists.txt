cmake_minimum_required(VERSION 3.4)

project(KangZiXin LANGUAGES C)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 99)

include (TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    message(STATUS "BIG_ENDIAN")
    # set(kzx_defines -D__BIG_ENDIAN__)
    add_definitions(-D__BIG_ENDIAN__)
else()
    message(STATUS "LITTLE_ENDIAN")
    # set(kzx_defines -D__LITTLE_ENDIAN__)
    add_definitions(-D__LITTLE_ENDIAN__)
endif()

set(kzx_sources src/main.c
    src/sys/struct/lineardb_test.c
    src/sys/struct/linearkv_test.c
    src/sys/struct/lineardb_pipe_test.c
)

set(kzx_defines "")
# set(kzx_cflags "")
set(kzx_cflags "-Wno-format")
# set(kzx_cflags "-Wformat")

add_executable(kang ${kzx_sources})
target_compile_definitions(kang PRIVATE ${kzx_defines})
target_compile_options(kang PRIVATE ${kzx_cflags})
target_include_directories(kang PRIVATE src/sys/struct)
target_link_libraries(kang m)
