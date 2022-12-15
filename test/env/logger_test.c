#include <env/env.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static __atombool running = __false;
__sym sym[26] = {'a', 'b', 'c', 'd', 'e', 'f'};

__result pipe_write_thread(__ptr p)
{
    __result result = 0;
	__sym s = 0;
	__sint32 size = 32;
	__sym *buf = NULL;
	env_pipe_t *pipe = (env_pipe_t *)p;
	// env_pipe_t *pipe = 123;

	for (int i = 0; i < 100000; ++i){
		buf = (__sym*)malloc(size);
		__pass(buf != NULL);
        s = i % 6;
		memset(buf, sym[s], size);
        result = snprintf(buf, 32, "%d 0x%X", i, env_thread_self());
        buf[result] = ' ';
        result = env_pipe_write(pipe, buf, size);
		free(buf);
	}

Reset:

	return 0;
}

__result pipe_read_thread(__ptr p)
{
	__result result = 0, count = 0;
	__sint32 size = 32;
	__sym *buf = NULL;
	env_pipe_t *pipe = (env_pipe_t *)p;
	// env_pipe_t *pipe = 1234;


	while (1){
		buf = (__sym*)malloc(size);
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
	return 0;
}

void logger_test()
{
	int result = 0;
	env_thread_t write_tid = 0;
	env_thread_t read_tid = 0;
    env_thread_t write_tid_1 = 0;
	env_thread_t read_tid_1 = 0;
	env_pipe_t *pipe = NULL;

	pipe = env_pipe_create(1<<14);

	__pass(pipe != NULL);

	running = __true;
	env_thread_create(&read_tid, pipe_read_thread, pipe);
	env_thread_create(&write_tid, pipe_write_thread, pipe);
    env_thread_create(&read_tid_1, pipe_read_thread, pipe);
	// env_thread_create(&write_tid_1, pipe_write_thread, pipe);

	__logd("phtread join write thread\n");

	env_thread_destroy(write_tid);
    // env_thread_destroy(write_tid_1);

	__logd("phtread join read thread\n");

    __set_false(running);
    // env_pipe_clear(pipe);
    env_pipe_stop(pipe);

	env_thread_destroy(read_tid);
    env_thread_destroy(read_tid_1);

	env_pipe_destroy(&pipe);

Reset:

	return;
}