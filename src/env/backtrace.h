//
// Created by liyong kang on 2022/12/1.
//

#ifndef __ENV_BACKTRACE_H__
#define __ENV_BACKTRACE_H__


#ifdef __ANDROID__

#include <unwind.h>

struct backtrace_stack {
    void** head;
    void** end;
};

static _Unwind_Reason_Code env_unwind_backtrace_callback(struct _Unwind_Context* unwind_context, void* p)
{
    struct backtrace_stack* stack = (struct backtrace_stack*)p;
    if (stack->head == stack->end) {
        return _URC_END_OF_STACK;
    }
    *stack->head = (void*)_Unwind_GetIP(unwind_context);
    if (*stack->head == NULL) {
        return _URC_END_OF_STACK;
    }
    ++stack->head;
    return _URC_NO_REASON;
}

static inline size_t env_backtrace(void** array, int depth)
{
    struct backtrace_stack stack = {0};
    stack.head = &array[0];
    stack.end = &array[0] + (depth - 2);
    _Unwind_Backtrace(env_unwind_backtrace_callback, &stack);
    return stack.head - &array[0];
}

#else

#include <execinfo.h>

static inline size_t env_backtrace(void** array, int depth)
{
    return backtrace(array, depth);
}

#endif

#endif //__ENV_UNWIND_H__
