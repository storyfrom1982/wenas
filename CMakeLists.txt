cmake_minimum_required(VERSION 3.4)

project(KangZiXin LANGUAGES C)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 90)

set(kzx_sources src/main.c)

set(kzx_defines "")
set(kzx_cflags "")

add_executable(kang ${kzx_sources})
target_compile_definitions(kang PRIVATE ${kzx_defines})
target_compile_options(kang PRIVATE ${kzx_cflags})
target_include_directories(kang PRIVATE src/sys/struct)
target_link_libraries(kang m)
