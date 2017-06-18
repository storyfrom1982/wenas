/*
 * message.c
 *
 *  Created on: 2017年6月18日
 *      Author: kly
 */


#include <sr_message.h>

#include <sr_log.h>
#include <sr_common.h>
#include <sr_pipe.h>
#include <sr_malloc.h>


static void notify(Sr_message_callback *cb, Sr_message msg)
{
	logd("message : %d  size %d\n", msg.event, msg.i32);
	if (msg.size > 0){
		char *data = malloc(msg.size + 1);
		memcpy(data, msg.data, msg.size);
		data[msg.size] = '\0';
		logd("message size >>>> %d %s end\n", msg.size, data);
//		free(msg.data);
		free(data);
	}
}


int main(int argc, char *argv[])
{
	sr_malloc_initialize(1024 * 1024 * 8, 2);

	char *tmp = NULL;
	int result = 0;

	int64_t start_time = sr_starting_time();

	Sr_message_listener *listener;

	Sr_message_callback cb;
	cb.notify = notify;
	sr_message_listener_create(&cb, &listener);


	for (int i = 0; i < 100; ++i){
		int size = random() % 4096;
		if (size < 31){
			size = 31;
		}
		Sr_message msg = {.event = i, .i32 = size, .size = size, .data = NULL};
		char data[size];
		msg.data = data;
		memset(msg.data, i, msg.size);
		sr_message_listener_push(listener, msg);
	}

	while (sr_message_listener_arrivals(listener)){
		usleep(1000000);
	}

	sr_message_listener_release(&listener);

	logd("used time %ld\n", sr_calculate_time(start_time));

	sr_malloc_debug(sr_log_info);

	sr_malloc_release();

	return 0;
}
