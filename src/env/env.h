#ifndef __ENV_ENV_H__
#define __ENV_ENV_H__

#ifndef _WIN32

#include "env/atomic.h"
#include "env/logger.h"
#include "env/malloc.h"
#include "env/backtrace.h"
#include "env/thread.h"
#include "env/file_system.h"
#include "env/task_queue.h"

#else

// #include <windows.h>
#include <stdio.h>

#endif

#endif