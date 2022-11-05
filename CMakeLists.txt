cmake_minimum_required(VERSION 3.4)

project(ImproveKang LANGUAGES C)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 90)

set(ik_sources src/main.c)

set(ik_defines "")
set(ik_cflags "")

add_executable(improve_kang ${ik_sources})
target_compile_definitions(improve_kang PRIVATE ${ik_defines})
target_compile_options(improve_kang PRIVATE ${ik_cflags})
target_link_libraries(improve_kang uv)
