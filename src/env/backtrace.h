//
// Created by liyong kang on 2022/12/1.
//

#ifndef __ENV_BACKTRACE_H__
#define __ENV_BACKTRACE_H__

#include <stddef.h>

extern size_t env_backtrace(void** array, int depth);
extern void env_backtrace_setup();

#endif //__ENV_UNWIND_H__
