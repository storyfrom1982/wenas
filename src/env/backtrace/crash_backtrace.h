#include <signal.h>
#include <unwind.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


#define ENV_BACKTRACE_STACK_DEPTH       256


struct BacktraceState {
    uintptr_t* current;
    uintptr_t* end;
};

static _Unwind_Reason_Code env_unwind_backtrace_callback(struct _Unwind_Context* unwind_context, void* state_voidp)
{
    struct BacktraceState* state = (struct BacktraceState*)state_voidp;
    *state->current = _Unwind_GetIP(unwind_context);
    ++state->current;
    if (!*(state->current - 1) || state->current == state->end) {
        return _URC_END_OF_STACK;
    }
    return _URC_NO_REASON;
}

static void env_crash_signal_handler(int sig, siginfo_t* info, void* ucontext)
{
    printf("Crash Thread [0x%x] ****** %s ******\n", pthread_self(), strsignal(sig));
    const ucontext_t* signal_ucontext = (const ucontext_t*)ucontext;
    uintptr_t stacks[ENV_BACKTRACE_STACK_DEPTH];
    struct BacktraceState backtrace_state = {0};
    backtrace_state.current = stacks;
    backtrace_state.end = stacks + ENV_BACKTRACE_STACK_DEPTH;
    _Unwind_Backtrace(env_unwind_backtrace_callback, &backtrace_state);
    size_t stack_depth = backtrace_state.current - &stacks[0];
    for (size_t i = 0; i < stack_depth; ++ i) {
        Dl_info info = {};
        if (dladdr((void*)(stacks[i]), &info) && info.dli_sname && info.dli_fname) {
            printf("Crash Stack #%02zu: %32s() %s\n", i, info.dli_sname, info.dli_fname);
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