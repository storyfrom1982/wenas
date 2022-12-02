cmake_minimum_required(VERSION 3.4)

project(KangZiXin LANGUAGES C)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)

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

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "CMAKE_SIZEOF_VOID_P EQUAL 8")
    add_definitions(-D__PL64__)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(STATUS "CMAKE_SIZEOF_VOID_P EQUAL 4")
    add_definitions(-D__PL32__)
endif()

include(CheckCSourceRuns)
check_c_source_runs("
    #include <dlfcn.h>
    #include <execinfo.h>
    int main() {
        Dl_info info= {0};
        void *p[1];
        backtrace(p, 1);
        dladdr(p[0], &info);
        return 0;
    }" ENV_HAVE_EXECINFO)

check_c_source_runs("
    #include <dlfcn.h>
    #include <signal.h>
    #include <unwind.h>
    #include <execinfo.h>
    static _Unwind_Reason_Code cb(struct _Unwind_Context* a, void* b){
        return _URC_NO_REASON;
    }
    int main() {
        Dl_info info= {0};
        void *p[1];
        backtrace(p, 1);
        dladdr(p[0], &info);
        _Unwind_Backtrace(cb, NULL);
        return 0;
    }" ENV_HAVE_UNWIND)

check_c_source_runs("
    #include <stdatomic.h>
    int main() {
        atomic_size_t a;
        atomic_fetch_add(&a, 1);
        return 0;
    }" ENV_HAVE_STDATOMIC)

set(kzx_sources src/main.c
        src/env/malloc.c
    test/sys/struct/heap_test.c
    test/sys/struct/lineardb_test.c
    test/sys/struct/linearkv_test.c
    test/sys/struct/lineardb_pipe_test.c
    test/env/thread_test.c
    test/env/task/task_queue_test.c
    test/env/malloc/malloc_test.c
    test/env/file_system_test.c
    test/env/logger_test.c
)

set(kzx_defines "")
# set(kzx_cflags "")
set(kzx_cflags "-Wno-format")
# set(kzx_cflags "-Wformat")
set(kzx_ldflags "")

if(ENV_HAVE_EXECINFO)
    list(APPEND kzx_cflags -DENV_HAVE_EXECINFO -DENV_MALLOC_BACKTRACE)
endif()

if(ENV_HAVE_UNWIND)
    list(APPEND kzx_sources test/env/backtrace/crash_backtrace_test.c)
endif()

if(ENV_HAVE_STDATOMIC)
    list(APPEND kzx_cflags -DENV_HAVE_STDATOMIC)
endif()

if (UNIX)    
    list(APPEND kzx_cflags -DSR_LOG_DEBUG -DENV_MALLOC_PAGE_SIZE=0xA00000 -DENV_MALLOC_MAX_POOL=16)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND kzx_ldflags pthread dl)
    endif()    
endif()

add_executable(kang ${kzx_sources})
target_compile_definitions(kang PRIVATE ${kzx_defines})
target_compile_options(kang PRIVATE ${kzx_cflags})
target_include_directories(kang PRIVATE
    src
    )
target_link_libraries(kang m ${kzx_ldflags})
