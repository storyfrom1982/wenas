#ifndef __ENV_ENV_H__
#define __ENV_ENV_H__


#include "env/platforms.h"
#include "env/disk.h"


#define __pass(condition) \
    do { \
        if (!(condition)) { \
            printf("Check condition failed: %s, %s\n", #condition, env_status()); \
            goto Reset; \
        } \
    } while (__false)

#endif //__ENV_ENV_H__