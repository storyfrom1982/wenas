#ifndef __ENV_WIN_H__
#define __ENV_WIN_H__


#include <stdint.h>
#include <stddef.h>

#include <stdio.h>


void win32_init(void);

uint64_t env_time(void);
uint64_t env_sys_time(void);


#endif