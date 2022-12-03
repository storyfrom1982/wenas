#include "backtrace.h"

#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unwind.h>

#include "env/logger.h"


struct backtrace_stack {
    void** head;
    void** end;
};

static _Unwind_Reason_Code env_unwind_backtrace_callback(struct _Unwind_Context* unwind_context, void* vp)
{
    struct backtrace_stack* stack = (struct backtrace_stack*)vp;
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

size_t env_backtrace(void** array, int depth)
{
    struct backtrace_stack stack = {0};
    stack.head = &array[0];
    stack.end = &array[0] + (depth - 2);
    _Unwind_Backtrace(env_unwind_backtrace_callback, &stack);
    return stack.head - &array[0];
}


#ifdef __PL64__
#   define ENV_BACKTRACE_FORMAT "Stack:  #%-4d%-32s  0x%016X  %s() + %lu\n"
#   define ENV_BACKTRACE_ADDRESS_LEN 18 /* 0x + 16 (no NUL) */
#else
#   define ENV_BACKTRACE_FORMAT "Stack:  #%-4d%-32s  0x%08X  %s() + %lu"
#   define ENV_BACKTRACE_ADDRESS_LEN 10 /* 0x + 8 (no NUL) */
#endif


#define __USE_GNU
#include <dlfcn.h>

static int env_backtrace_printf(int frame, const void* addr, const Dl_info* info)
{
	char symbuf[ENV_BACKTRACE_ADDRESS_LEN + 1];
	const char* image = "???";
	const char* symbol = "0x0";
	int64_t symbol_offset = 0;

	if (info->dli_fname) {
		const char *tmp = strrchr(info->dli_fname, '/');
		if(tmp == NULL)
			image = info->dli_fname;
		else
			image = tmp + 1;
	}
	
	if (info->dli_sname) {
		symbol = info->dli_sname;
		symbol_offset = addr - info->dli_saddr;
	} else if(info->dli_fname) {
		symbol = image;
		symbol_offset = addr - info->dli_fbase;
	} else if(0 < snprintf(symbuf, sizeof(symbuf), "0x%X", info->dli_saddr)) {
		symbol = symbuf;
		symbol_offset = addr - info->dli_saddr;
	}

	LOGE("Backtrace",
			ENV_BACKTRACE_FORMAT,
			frame,
			image,
			addr,
			symbol,
			symbol_offset);

    return 0;
}

#define ENV_BACKTRACE_STACK_DEPTH       256

static void env_crash_signal_handler(int sig, siginfo_t* info, void* ucontext)
{
    LOGE("CRASH", "****** %s ****** Thread [0x%x] ******\n", strsignal(sig), pthread_self());
    const ucontext_t* signal_ucontext = (const ucontext_t*)ucontext;
    void *stacks[ENV_BACKTRACE_STACK_DEPTH];
    size_t stack_depth = env_backtrace(stacks, ENV_BACKTRACE_STACK_DEPTH);
    for (size_t i = 0; i < stack_depth; ++ i) {
        Dl_info info = {0};
        if (dladdr((stacks[i]), &info) && info.dli_sname) {
            env_backtrace_printf(i, (const void*)stacks[i], &info);
        }
    }
    exit(0);
}

void env_backtrace_setup()
{
    struct sigaction action = {};
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = env_crash_signal_handler;
    action.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;

    sigaction(SIGILL, &action, NULL);
    sigaction(SIGABRT, &action, NULL);
    sigaction(SIGBUS, &action, NULL);
    sigaction(SIGFPE, &action, NULL);
    sigaction(SIGSEGV, &action, NULL);
}
