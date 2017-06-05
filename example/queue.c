/*
 * queue.c
 *
 *  Created on: 2017年3月1日
 *      Author: kly
 */


#include <sr_log.h>
#include <sr_queue.h>
#include <sr_malloc.h>


typedef struct SR_TestNode{
	int id;
	SR_QUEUE_ENABLE(SR_TestNode);
}SR_TestNode;

SR_QUEUE_DEFINE(SR_TestNode);

SR_QUEUE_DECLARE(SR_TestNode) queue;



int main(int argc, char *argv[])
{
	sr_malloc_initialize(1024 * 1024 * 8, 2);

	int result;
	int size = 1024;

	SR_TestNode *frame = NULL;

	sr_queue_initialize(&queue, size);


	for (int i = 0; i < 100; ++i){
		frame = (SR_TestNode *)malloc(sizeof(SR_TestNode));
		frame->id = i;
		sr_queue_push_back(&queue, frame, result);
	}

	for (int i = 0; i < 50; ++i){
		sr_queue_pop_front(&queue, frame, result);
		logd("frame id = %d\n", frame->id);
		free(frame);
	}


	sr_malloc_debug(sr_log_info);

	logw("queue popable == %d\n", sr_queue_popable(&queue));

	sr_queue_clean(&queue);

	logd("queue popable == %d\n", sr_queue_popable(&queue));

	strdup("test memory leak");

	sr_malloc_debug(sr_log_info);

	sr_malloc_release();

	return 0;
}
