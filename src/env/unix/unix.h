#ifndef __ENV_UNIX_H__
#define __ENV_UNIX_H__


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <stdio.h>

void unix_init(void);

uint64_t env_time(void);
uint64_t env_sys_time(void);

int env_status(void);
char* env_status_describe(int status);



#endif // __ENV_UNIX_H__