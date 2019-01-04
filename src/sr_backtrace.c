/**
 * copy from https://github.com/alexeikh/android-ndk-backtrace-test.git
 */

#include "sr_library.h"

#include <unwind.h>

#include <assert.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


typedef void (*__log_debug)(const char *format, ...);

static __log_debug log_debug = NULL;
static const size_t address_count_max = 30;
static char g_stack_buffer[SIGSTKSZ] = {0};

struct BacktraceState {
    // On ARM32 architecture this context is needed
    // for getting backtrace of the before-crash stack,
    // not of the signal handler stack.
    const ucontext_t*   signal_ucontext;

    // On non-ARM32 architectures signal handler stack
    // seems to be "connected" to the before-crash stack,
    // so we only need to skip several initial addresses.
    size_t              address_skip_count;

    size_t              address_count;
    uintptr_t           addresses[address_count_max];

};
typedef struct BacktraceState BacktraceState;


static void BacktraceState_Init(BacktraceState* state, const ucontext_t* ucontext) {
    assert(state);
    assert(ucontext);
    memset(state, 0, sizeof(BacktraceState));
    state->signal_ucontext = ucontext;
    state->address_skip_count = 3;
}

static bool BacktraceState_AddAddress(BacktraceState* state, uintptr_t ip) {
    assert(state);

    // No more space in the storage. Fail.
    if (state->address_count >= address_count_max)
        return false;

#if __thumb__
    // Reset the Thumb bit, if it is set.
    const uintptr_t thumb_bit = 1;
    ip &= ~thumb_bit;
#endif

    if (state->address_count > 0) {
        // Ignore null addresses.
        // They sometimes happen when using _Unwind_Backtrace()
        // with the compiler optimizations,
        // when the Link Register is overwritten by the inner
        // stack frames, like PreCrash() functions in this example.
        if (ip == 0)
            return true;

        // Ignore duplicate addresses.
        // They sometimes happen when using _Unwind_Backtrace()
        // with the compiler optimizations,
        // because we both add the second address from the Link Register
        // in ProcessRegisters() and receive the same address
        // in UnwindBacktraceCallback().
        if (ip == state->addresses[state->address_count - 1])
            return true;
    }

    // Finally add the address to the storage.
    state->addresses[state->address_count++] = ip;
    return true;
}


static _Unwind_Reason_Code UnwindBacktraceWithSkippingCallback(
        struct _Unwind_Context* unwind_context, void* state_voidp) {
    assert(unwind_context);
    assert(state_voidp);

    BacktraceState* state = (BacktraceState*)state_voidp;
    assert(state);

    // Skip some initial addresses, because they belong
    // to the signal handler frame.
    if (state->address_skip_count > 0) {
        state->address_skip_count--;
        return _URC_NO_REASON;
    }

    uintptr_t ip = _Unwind_GetIP(unwind_context);
    bool ok = BacktraceState_AddAddress(state, ip);
    if (!ok)
        return _URC_END_OF_STACK;

    return _URC_NO_REASON;
}


static void UnwindBacktraceWithSkipping(BacktraceState* state) {
    assert(state);
    _Unwind_Backtrace(UnwindBacktraceWithSkippingCallback, state);
}


static void PrintBacktrace(BacktraceState* state) {
    assert(state);

    size_t frame_count = state->address_count;
    for (size_t frame_index = 0; frame_index < frame_count; ++frame_index) {

        void* address = (void*)(state->addresses[frame_index]);
        assert(address);

        const char* symbol_name = "";

        Dl_info info = {};
        if (dladdr(address, &info) && info.dli_sname) {
            symbol_name = info.dli_sname;
        }

        // Relative address matches the address which "nm" and "objdump"
        // utilities give you, if you compiled position-independent code
        // (-fPIC, -pie).
        // Android requires position-independent code since Android 5.0.
        unsigned long relative_address = (char*)address - (char*)info.dli_fbase;

        assert(symbol_name);
        if (log_debug){
            log_debug("crash #%02zu:  0x%lx  %s\n", frame_index, relative_address, symbol_name);
        }
    }
}


static void SigActionHandler(int sig, siginfo_t* info, void* ucontext) {
    const ucontext_t* signal_ucontext = (const ucontext_t*)ucontext;
    assert(signal_ucontext);
    BacktraceState backtrace_state = {0};
    BacktraceState_Init(&backtrace_state, signal_ucontext);
    UnwindBacktraceWithSkipping(&backtrace_state);
    PrintBacktrace(&backtrace_state);
    exit(0);
}


static void SetUpAltStack() {
    // Set up an alternate signal handler stack.
    stack_t stack = {};
    stack.ss_size = 0;
    stack.ss_flags = 0;
    stack.ss_size = SIGSTKSZ;
    stack.ss_sp = g_stack_buffer;
    assert(stack.ss_sp);

    sigaltstack(&stack, NULL);
}


static void SetUpSigActionHandler() {
    // Set up signal handler.
    struct sigaction action = {};
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = SigActionHandler;
    action.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;

    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGABRT, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGILL, &action, NULL);
    sigaction(SIGPIPE, &action, NULL);
}


void sr_setup_crash_backtrace(void (*log_callback)(const char *format, ...))
{
    log_debug = log_callback;
    SetUpAltStack();
    SetUpSigActionHandler();
}