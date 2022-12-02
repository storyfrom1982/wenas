#ifndef __ENV_CRASH_BACKTRACE_H__
#define __ENV_CRASH_BACKTRACE_H__

#include <signal.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "env/logger.h"
#include "env/backtrace.h"


#define ENV_BACKTRACE_STACK_DEPTH       256


#ifdef __PL64__
#   define ENV_BACKTRACE_FORMAT "Stack:  #%-4d%-32s  0x%016lx  %s() + %lu\n"
#   define ENV_BACKTRACE_ADDRESS_LEN 18 /* 0x + 16 (no NUL) */
#else
#   define ENV_BACKTRACE_FORMAT "Stack:  #%-4d%-32s  0x%08lx  %s() + %lu"
#   define ENV_BACKTRACE_ADDRESS_LEN 10 /* 0x + 8 (no NUL) */
#endif


static int env_backtrace_printf(int frame, const void* addr, const Dl_info* info)
{
	char symbuf[ENV_BACKTRACE_ADDRESS_LEN + 1];
	const char* image = "???";
	const char* symbol = "0x0";
	uintptr_t symbol_offset = 0;

	if (info->dli_fname) {
		const char *tmp = strrchr(info->dli_fname, '/');
		if(tmp == NULL)
			image = info->dli_fname;
		else
			image = tmp + 1;
	}
	
	if (info->dli_sname) {
		symbol = info->dli_sname;
		symbol_offset = (uintptr_t)addr - (uintptr_t)info->dli_saddr;
	} else if(info->dli_fname) {
		symbol = image;
		symbol_offset = (uintptr_t)addr - (uintptr_t)info->dli_fbase;
	} else if(0 < snprintf(symbuf, sizeof(symbuf), "0x%lx", (uintptr_t)info->dli_saddr)) {
		symbol = symbuf;
		symbol_offset = (uintptr_t)addr - (uintptr_t)info->dli_saddr;
	} else {
		symbol_offset = (uintptr_t)addr;
	}

	LOGD("CRASH",
			ENV_BACKTRACE_FORMAT,
			frame,
			image,
			(uintptr_t)addr,
			symbol,
			symbol_offset);

    return 0;
}

static void env_crash_signal_handler(int sig, siginfo_t* info, void* ucontext)
{
    LOGE("CRASH", "****** %s ****** Thread [0x%x] ******\n", strsignal(sig), pthread_self());
    const ucontext_t* signal_ucontext = (const ucontext_t*)ucontext;
    void *stacks[ENV_BACKTRACE_STACK_DEPTH];
    size_t stack_depth = env_backtrace(stacks, ENV_BACKTRACE_STACK_DEPTH);
    for (size_t i = 0; i < stack_depth; ++ i) {
        Dl_info info = {};
        if (dladdr((void*)(stacks[i]), &info) && info.dli_sname) {
            env_backtrace_printf(i, (const void*)stacks[i], &info);
        }
    }
    exit(0);
}

static inline void env_crash_backtrace_setup()
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

#endif