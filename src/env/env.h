#ifndef __ENV_ENV_H__
#define __ENV_ENV_H__

#ifdef _WIN64
#   define OS_WINDOWS
#endif

#ifdef __APPLE__
#   define OS_APPLE
#endif

#ifdef __ANDROID__
#   define OS_ANDROID
#endif

///////////////////////////////////////////////////////
///// 2进制
///////////////////////////////////////////////////////
#define __false                 0
#define __true                  1
typedef unsigned char           __bit;

///////////////////////////////////////////////////////
///// 256进制符号
///////////////////////////////////////////////////////
typedef char                    __sym;
typedef char*                   __symptr;

///////////////////////////////////////////////////////
///// 自然数集
///////////////////////////////////////////////////////
typedef unsigned char           __uint8;
typedef unsigned short          __uint16;
typedef unsigned long           __uint32;
typedef unsigned long long      __uint64;

///////////////////////////////////////////////////////
///// 整数集
///////////////////////////////////////////////////////
typedef char                    __sint8;
typedef short                   __sint16;
typedef long                    __sint32;
typedef long long               __sint64;

///////////////////////////////////////////////////////
///// 实数集
///////////////////////////////////////////////////////
typedef float                   __real32;
typedef double                  __real64;

///////////////////////////////////////////////////////
///// 可变类型
///////////////////////////////////////////////////////
typedef void                    __void;

///////////////////////////////////////////////////////
///// 可变类型指针
///////////////////////////////////////////////////////
typedef void*                   __ptr;

///////////////////////////////////////////////////////
///// 当前状态
///////////////////////////////////////////////////////
const __sym* env_status(void);

///////////////////////////////////////////////////////
///// 时间相关
///////////////////////////////////////////////////////
#define MILLI_SECONDS    1000ULL
#define MICRO_SECONDS    1000000ULL
#define NANO_SECONDS     1000000000ULL

__uint64 env_time(void);
__uint64 env_clock(void);

///////////////////////////////////////////////////////
///// 存储相关
///////////////////////////////////////////////////////
typedef void*   __fp;

__fp env_fopen(const __symptr path, const __symptr mode);
__bit env_fclose(__fp fp);
__sint64 env_ftell(__fp fp);
__sint64 env_fflush(__fp fp);
__sint64 env_fwrite(__fp fp, __ptr data, __uint64 size);
__sint64 env_fread(__fp fp, __ptr buf, __uint64 size);
__sint64 env_fseek(__fp fp, __sint64 offset, __sint32 whence);

__bit env_make_path(const __symptr path);
__bit env_find_path(const __symptr path);
__bit env_find_file(const __symptr path);
__bit env_remove_path(const __symptr path);
__bit env_remove_file(const __symptr path);
__bit env_move_path(const __symptr from, const __symptr to);


// #define __pass(condition) \
//     do { \
//         if (!(condition)) { \
//             goto Reset; \
//         } \
//     } while (__false)

#endif //__ENV_ENV_H__