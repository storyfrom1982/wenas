#include <env/env.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static __atombool running = __false;
char sym[26] = {'a', 'b', 'c', 'd', 'e', 'f'};

__sint32 pipe_write_thread(__ptr p)
{
    __sint32 result = 0;
	char s = 0;
	__sint32 size = 32;
	char *buf = NULL;
	env_pipe_t *pipe = (env_pipe_t *)p;
	// pipe = 123;

	for (int i = 0; i < 100000; ++i){
		buf = (char*)malloc(size);
		__pass(buf != NULL);
        s = i % 6;
		memset(buf, sym[s], size);
        result = snprintf(buf, 32, "%d 0x%X", i, env_thread_self());
        buf[result] = ' ';
        result = env_pipe_write(pipe, buf, size);
		free(buf);
		if (result != size) {
			break;
		}
	}

Reset:
	__logd("pipe_write_thread exit ================== 0x%X\n", env_thread_self());
	return 0;
}

__sint32 pipe_read_thread(__ptr p)
{
	__sint32 result = 0, count = 0;
	__sint32 size = 32;
	char *buf = NULL;
	env_pipe_t *pipe = (env_pipe_t *)p;

	while (1){
		
		buf = (char*)malloc(size);
        __pass(buf != NULL);
        result = env_pipe_read(pipe, buf, size);
        if (result != size && __is_false(running)){
            free(buf);
            break;
        }
		buf[size - 1] = '\0';
		__logd("%d=%s\n", result, buf);
		free(buf);
	}

Reset:
	__logd("pipe_read_thread exit --------------------- 0x%X\n", env_thread_self());
	return 0;
}

void logger_test()
{
	int result = 0;
	env_thread_ptr write_tid = 0;
	env_thread_ptr read_tid = 0;
    env_thread_ptr write_tid_1 = 0;
	env_thread_ptr read_tid_1 = 0;
	env_pipe_t *pipe = NULL;
	env_pipe_t *pipe_1 = NULL;

	pipe = env_pipe_create(1<<4);
	pipe_1 = env_pipe_create(1<<19);

	__pass(pipe != NULL);

	running = __true;
	env_thread_create(&read_tid, pipe_read_thread, pipe);
	env_thread_create(&write_tid, pipe_write_thread, pipe);
    env_thread_create(&read_tid_1, pipe_read_thread, pipe_1);
	env_thread_create(&write_tid_1, pipe_write_thread, pipe_1);

	__logd("phtread join write =============================== thread\n");

	//env_thread_destroy(write_tid);
    //env_thread_destroy(write_tid_1);

	env_thread_sleep(3000000000);

	__logd("phtread stopping read thread\n");

    __set_false(running);
    env_pipe_clear(pipe);
	env_pipe_clear(pipe_1);
	__logd("env_pipe_stop =============================== 1\n");
    env_pipe_stop(pipe);
	__logd("env_pipe_stop =============================== 2\n");
	env_pipe_stop(pipe_1);
	__logd("env_pipe_stop =============================== 3\n");

	 env_thread_destroy(write_tid);
     env_thread_destroy(write_tid_1);

	__logd("phtread join read thread\n");

	env_thread_destroy(read_tid);
    env_thread_destroy(read_tid_1);

	env_pipe_destroy(&pipe);
	env_pipe_destroy(&pipe_1);

Reset:

	return;
}