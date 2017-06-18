/*
 * memory_leak.c
 *
 *  Created on: 2017年3月1日
 *      Author: kly
 */

#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>

#include <sr_log.h>
#include <sr_common.h>
#include <sr_queue.h>
#include <sr_malloc.h>



typedef struct PacketNode{
	Sr_node *prev;
	Sr_node *next;
	int id;
	size_t size;
	uint8_t *data;
}PacketNode;

typedef struct Task{
	int id;
	pthread_t producer;
	pthread_t consumers;
	Sr_queue *queue;
}Task;


static uint64_t malloc_count = 0;


static void* producer(void *p)
{
	int result = 0;

	void *addres = malloc(1024 * 1024 * 4);
	if (addres == NULL){
		loge(ERRMALLOC);
		exit(0);
		return NULL;
	}

	PacketNode *pkt = NULL;
	Task *task = (Task*)p;

	for(int i = 0; i < 100000; ++i){
		pkt = (PacketNode*)malloc(sizeof(PacketNode));
		if (pkt == NULL){
			loge(ERRMALLOC);
			exit(0);
			break;
		}
		SR_ATOM_ADD(malloc_count, 1);
		pkt->id = task->id;
		pkt->size = random() % 10240;
		if (pkt->size < 128){
			pkt->size = 128;
		}
		pkt->data = (uint8_t*)malloc(pkt->size);
//		pkt->data = (uint8_t*)calloc(1, pkt->size);
		if (pkt->data == NULL){
			loge(ERRMALLOC);
			exit(0);
			break;
		}
		SR_ATOM_ADD(malloc_count, 1);
		snprintf(pkt->data, pkt->size, "producer id = %d malloc size = %lu", task->id, pkt->size);
//		pkt->data = (uint8_t*)realloc(pkt->data, pkt->size + 1);

		while((result = srq_push_back(task->queue, pkt)) == QUEUE_RESULT_TRYAGAIN) {
			nanosleep((const struct timespec[]){{0, 100L}}, NULL);
		}

		if (result != 0){
			loge(result);
			free(pkt->data);
			free(pkt);
			break;
		}
	}


	sr_queue_stop(task->queue);

//	free(addres);

	return NULL;
}

static void* consumers(void *p)
{
	int result = 0;

	PacketNode *pkt = NULL;
	Task *task = (Task*)p;

	while(true){

		while((result = srq_get_front(task->queue, pkt)) == QUEUE_RESULT_TRYAGAIN) {
			nanosleep((const struct timespec[]){{0, 100L}}, NULL);
		}

		if (result == 0){
			result = srq_remove(task->queue, pkt);
			if (result != 0){
				loge(result);
			}
		}

		if (result == 0){
//			logd("%s\n", pkt->data);
			free(pkt->data);
			free(pkt);
		}else{
			logd("tid %ld exit queue popable %lu\n", pthread_self(), sr_queue_popable(task->queue));
			break;
		}
	}

	return NULL;
}


static void clean(Sr_node *p){
	if (p != NULL){
		PacketNode *pkt = (PacketNode*)p;
		free(pkt->data);
		free(pkt);
	}
}


void* malloc_test(int producer_count, int consumers_count)
{
	int i = 0;
	int size = 1024000;
	int result = 0;
	int c_number = consumers_count;
	int p_number = producer_count;

	int64_t start_time = sr_starting_time();

	Task plist[p_number];
	Task clist[c_number];


	for (i = 0; i < c_number; ++i){
		clist[i].id = i;
		clist[i].queue = sr_queue_create(size);
		pthread_create(&(clist[i].consumers), NULL, consumers, &(clist[i]));
	}

	for (i = 0; i < p_number; ++i){
		plist[i].id = i;
		plist[i].queue = clist[i].queue;
		pthread_create(&(plist[i].producer), NULL, producer, &(plist[i]));
	}


	for (i = 0; i < p_number; ++i){
		pthread_join(plist[i].producer, NULL);
	}


	for (i = 0; i < c_number; ++i){
		pthread_join(clist[i].consumers, NULL);
		sr_queue_set_clean_callback(clist[i].queue, clean);
		sr_queue_clean(clist[i].queue);
	}


	logd("used time %ld\n", sr_calculate_time(start_time));

	sr_malloc_debug(sr_log_info);


	return NULL;
}




int main(int argc, char *argv[])
{
	sr_malloc_initialize(1024 * 1024 * 8, 2);

	char *tmp = NULL;
	int result = 0;

	int64_t start_time = sr_starting_time();

	for (int i = 0; i < 10; ++i){
		malloc_test(10, 10);
		logi("malloc test ============================= %d\n", i);
	}

	logw("used time %lu %ld\n", malloc_count, sr_calculate_time(start_time));

	sr_malloc_release();

	return 0;
}
