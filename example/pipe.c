/*
 * pipe.c
 *
 *  Created on: 2017年3月1日
 *      Author: kly
 */


#include <sr_lib.h>
#include <sr_malloc.h>

void* pipe_write_thread(void *p)
{
	int size = 32;
	char *buf = NULL;
	sr_pipe_t *pipe = (sr_pipe_t *)p;

	for (int i = 0; i < 100000; ++i){
		buf = (char*)malloc(size);
		if (buf == NULL){
			loge("malloc failed\n");
		}
		memset(buf, i % 0xFF, size);
		for(unsigned int result = 0; result < size; ){
			result += sr_pipe_write(pipe, buf + result, size - result);
		}
		free(buf);
	}

	sr_pipe_finish(pipe);

	return NULL;
}

void* pipe_read_thread(void *p)
{
	unsigned int result = 0;
	int size = 32;
	char *buf = NULL;
	sr_pipe_t *pipe = (sr_pipe_t *)p;

	bool running = true;
	while (running){
		buf = (char*)malloc(size);
		if (buf == NULL){
			loge("malloc failed\n");
		}
		for (result = 0; result < size;){
			result += sr_pipe_read(pipe, buf + result, size - result);
			if (result != size){
				if (sr_pipe_is_stopped(pipe)){
					running = false;
					break;
				}
			}
		}
		if (result > 0){
			buf[result - 1] = '\0';
			logd("%d=%s\n", result, buf);
		}
		free(buf);
	}

	return NULL;
}

int pipe_test()
{
	int result = 0;
	pthread_t write_tid = 0;
	pthread_t read_tid = 0;
	sr_pipe_t *pipe = NULL;

	int64_t start_time = sr_time_begin();

	pipe = sr_pipe_create(10240);

	if (result != 0){
		loge("sr_pipe_create failed\n");
		return result;
	}

	pthread_create(&read_tid, NULL, pipe_read_thread, pipe);
	pthread_create(&write_tid, NULL, pipe_write_thread, pipe);

	logd("phtread join write thread\n");

	pthread_join(write_tid, NULL);

	logd("phtread join read thread\n");

	pthread_join(read_tid, NULL);

	sr_pipe_release(&pipe);

	logd("once time %ld\n", sr_time_passed(start_time));

	sr_malloc_debug(sr_log_warn);

	return 0;
}


int main(int argc, char *argv[])
{
	sr_malloc_initialize(1024 * 1024 * 8, 2);

	char *tmp = NULL;
	int result = 0;

	int64_t start_time = sr_time_begin();

	for (int i = 0; i < 10; ++i){
		pipe_test();
		logd("pipe test ============================= %d\n", i);
	}

	logd("used time %ld\n", sr_time_passed(start_time));

	sr_malloc_release();

	return 0;
}
