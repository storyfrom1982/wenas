/*
 * malloc.c
 *
 *  Created on: 2017年3月1日
 *      Author: kly
 */


#include <stdio.h>
#include <pthread.h>

#include <sr_log.h>
#include <sr_time.h>
#include <sr_queue.h>
#include <sr_malloc.h>



typedef struct Packet_node{
	int id;
	size_t size;
	uint8_t *data;
	SR_QUEUE_ENABLE(Packet_node);
}Packet_node;

SR_QUEUE_DEFINE(Packet_node);


typedef struct Task{
	int id;
	pthread_t producer;
	pthread_t consumers;
	SR_QUEUE_DECLARE(Packet_node) *queue;
}Task;


static uint64_t malloc_count = 0;


static void* producer(void *p)
{
	int result = 0;
	size_t aligned = sizeof(size_t);
	aligned = 1024;

	void *add = aligned_alloc(aligned, 1024 * 1024 * 4);
	if (add == NULL){
		loge(ERRMALLOC);
		exit(0);
		return NULL;
	}

	Packet_node *pkt = NULL;
	Task *task = (Task*)p;

	for(int i = 0; i < 100000; ++i){
		pkt = (Packet_node*)malloc(sizeof(Packet_node));
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
//		pkt->size = pkt->size + aligned & ~(aligned-1);
		pkt->data = (uint8_t*)aligned_alloc(aligned, pkt->size);
//		pkt->data = (uint8_t*)calloc(1, pkt->size);
		if (pkt->data == NULL){
			loge(ERRMALLOC);
			exit(0);
			break;
		}
		SR_ATOM_ADD(malloc_count, 1);
//		snprintf(pkt->data, pkt->size, "producer id = %d malloc size = %lu", task->id, pkt->size);
//		pkt->data = (uint8_t*)realloc(pkt->data, pkt->size + 1);

		do {
			sr_queue_push_back(task->queue, pkt, result);
			if (result == ERRTRYAGAIN){
				nanosleep((const struct timespec[]){{0, 100L}}, NULL);
			}
		}while(result == ERRTRYAGAIN);

		if (result != 0){
			free(pkt->data);
			free(pkt);
		}
	}


	sr_queue_stop(task->queue);

//	free(add);

	pthread_exit(0);

	return NULL;
}

static void* consumers(void *p)
{
	int result = 0;

	Packet_node *pkt = NULL;
	Task *task = (Task*)p;

	while(true){

		do {
			sr_queue_pop_front(task->queue, pkt, result);
			if (result == ERRTRYAGAIN){
				nanosleep((const struct timespec[]){{0, 100L}}, NULL);
			}
		}while(result == ERRTRYAGAIN);

		if (result == 0){
//			logd("%s\n", pkt->data);
			free(pkt->data);
			free(pkt);
		}else{
			logd("tid %ld exit\n", pthread_self());
			break;
		}
	}

	pthread_exit(0);

	return NULL;
}


void* malloc_test(int producer_count, int consumers_count)
{
	int i = 0;
	int size = 1024000;
	int result = 0;
	int c_number = consumers_count;
	int p_number = producer_count;

//	sr_memory_default_init();

	int64_t start_time = sr_starting_time();

	SR_QUEUE_DECLARE(Packet_node) queue;
	Task plist[p_number];
	Task clist[c_number];

	sr_queue_initialize(&(queue), size);

	for (i = 0; i < c_number; ++i){
		clist[i].id = i;
		clist[i].queue = &queue;
		pthread_create(&(clist[i].consumers), NULL, consumers, &(clist[i]));
	}

	for (i = 0; i < p_number; ++i){
		plist[i].id = i;
		plist[i].queue = &queue;
		pthread_create(&(plist[i].producer), NULL, producer, &(plist[i]));
	}


	for (i = 0; i < p_number; ++i){
		pthread_join(plist[i].producer, NULL);
	}


	for (i = 0; i < c_number; ++i){
		pthread_join(clist[i].consumers, NULL);
	}


	logd("used time %ld\n", sr_calculate_time(start_time));

//	sr_memory_debug(sr_log_info);

//	sr_memory_release();

	return NULL;
}


int main(int argc, char *argv[])
{
	sr_malloc_initialize(1024 * 1024 * 8, 2);

//	void *ppp = malloc(1024);
//	logd("ppp ========== %p\n", ppp);

	char *tmp = NULL;
	int result = 0;

	int64_t start_time = sr_starting_time();

	for (int i = 0; i < 10; ++i){
		malloc_test(2, 2);
		logi("malloc test ============================= %d\n", i);
	}

	sr_malloc_debug(sr_log_info);

	pthread_exit(0);

	sr_malloc_release();

	logw("used time %lu %ld\n", malloc_count, sr_calculate_time(start_time));

	return 0;
}
