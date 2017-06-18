/*
 * pipe.c
 *
 *  Created on: 2017年3月1日
 *      Author: kly
 */


#include <sr_log.h>
#include <sr_common.h>
#include <sr_pipe.h>
#include <sr_malloc.h>

#include <pthread.h>

static bool running = false;

void* pipe_write_thread(void *p)
{
	int result = 0;
	int size = 32;
	char *buf = NULL;
	Sr_pipe *pipe = (Sr_pipe *)p;

	for (int i = 0; i < 100000; ++i){
		buf = (char*)malloc(size);
		if (buf == NULL){
			loge(ERRMALLOC);
		}
		memset(buf, i % 0xFF, size);
		for(result = 0; result < size; ){
			result += sr_pipe_write(pipe, buf + result, size - result);
		}
		free(buf);
	}

	SETFALSE(running);

	return NULL;
}

void* pipe_read_thread(void *p)
{
	int result = 0;
	int size = 32;
	char *buf = NULL;
	Sr_pipe *pipe = (Sr_pipe *)p;


	while (ISTRUE(running)){
		buf = (char*)malloc(size);
		if (buf == NULL){
			loge(ERRMALLOC);
		}
		for (result = 0; result < size;){
			result += sr_pipe_read(pipe, buf + result, size - result);
		}
		buf[size - 1] = '\0';
		logd("%d=%s\n", result, buf);
		free(buf);
	}

	return NULL;
}

int pipe_test()
{
	int result = 0;
	pthread_t write_tid = 0;
	pthread_t read_tid = 0;
	Sr_pipe *pipe = NULL;

	int64_t start_time = sr_starting_time();

	result = sr_pipe_create(10240, &pipe);

	if (result != 0){
		loge(result);
		return result;
	}

	running = true;
	pthread_create(&read_tid, NULL, pipe_read_thread, pipe);
	pthread_create(&write_tid, NULL, pipe_write_thread, pipe);

	logd("phtread join write thread\n");

	pthread_join(write_tid, NULL);

	logd("phtread join read thread\n");

	pthread_join(read_tid, NULL);

	sr_pipe_release(&pipe);

	logd("once time %ld\n", sr_calculate_time(start_time));

	sr_malloc_debug(sr_log_info);

	return 0;
}


int main(int argc, char *argv[])
{
	sr_malloc_initialize(1024 * 1024 * 8, 2);

	char *tmp = NULL;
	int result = 0;

	int64_t start_time = sr_starting_time();

	for (int i = 0; i < 10; ++i){
		pipe_test();
		logd("pipe test ============================= %d\n", i);
	}

	logd("used time %ld\n", sr_calculate_time(start_time));

	sr_malloc_release();

	return 0;
}
