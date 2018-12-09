/*
 * message.c
 *
 *  Created on: 2017年6月18日
 *      Author: kly
 */


#include <sr_lib.h>
#include <sr_malloc.h>


static void notify(sr_messenger_callback_t *cb, sr_message_t msg)
{
	logd("message : %d  size %lld\n", msg.type, msg.if64.int64);
	if (msg.extra_size > 0){
		char *data = malloc(msg.extra_size + 1);
		memcpy(data, msg.extra, msg.extra_size);
		data[msg.extra_size] = '\0';
		logd("message size >>>> %d %s end\n", msg.extra_size, data);
		free(data);
	}
}


int main(int argc, char *argv[])
{
	sr_malloc_initialize(1024 * 1024 * 8, 2);

	char *tmp = NULL;
	int result = 0;

	int64_t start_time = sr_time_begin();

	sr_messenger_t *messenger;

	sr_messenger_callback_t cb;
	cb.notify = notify;
	messenger = sr_messenger_create(&cb);

	int log_size = 512;

	for (int i = 0; i < 100; ++i){
		int size = random() % (log_size>>1);
		if (size < 31){
			size = 31;
		}
		sr_message_t msg = {.type = i, .if64.int64 = size, .extra_size = size, .extra = NULL};
		char data[size];
		msg.extra = data;
		memset(msg.extra, i, msg.extra_size);
		sr_messenger_send(messenger, msg);
	}

	sr_messenger_finish(messenger);

	while (sr_messenger_is_arrive(messenger)){
		usleep(1000000);
	}

	sr_messenger_release(&messenger);

	logd("used time %ld\n", sr_time_passed(start_time));

	sr_malloc_debug(sr_log_warn);

	sr_malloc_release();

	return 0;
}
