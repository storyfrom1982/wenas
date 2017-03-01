/*
 * memory_leak.c
 *
 *  Created on: 2017年3月1日
 *      Author: kly
 */

#include <stdio.h>
#include <pthread.h>

#include <sr_log.h>
#include <sr_time.h>
#include <sr_queue.h>
#include <sr_memory.h>



typedef struct Packet{
	int id;
	size_t size;
	uint8_t *data;
	SR_QUEUE_ENABLE(Packet);
}Packet;

SR_QUEUE_DEFINE(Packet);

typedef struct Task{
	int id;
	pthread_t producer;
	pthread_t consumers;
	SR_QUEUE_DECLARE(Packet) q, *queue;
}Task;


static uint64_t malloc_count = 0;


static void* producer(void *p)
{
	int result = 0;

	void *addres = malloc(1024 * 1024 * 4);
	if (addres == NULL){
		loge(ERRMEMORY);
		exit(0);
		return NULL;
	}

	Packet *pkt = NULL;
	Task *task = (Task*)p;

	for(int i = 0; i < 100000; ++i){
		pkt = (Packet*)malloc(sizeof(Packet));
		if (pkt == NULL){
			loge(ERRMEMORY);
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
			loge(ERRMEMORY);
			exit(0);
			break;
		}
		SR_ATOM_ADD(malloc_count, 1);
		snprintf(pkt->data, pkt->size, "producer id = %d malloc size = %lu", task->id, pkt->size);
//		pkt->data = (uint8_t*)realloc(pkt->data, pkt->size + 1);

		do {
			sr_queue_push_to_end(task->queue, pkt, result);
			if (result == ERRTRYAGAIN){
				nanosleep((const struct timespec[]){{0, 100L}}, NULL);
			}
		}while(result == ERRTRYAGAIN);

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

	Packet *pkt = NULL;
	Task *task = (Task*)p;

	while(true){

		do {
			sr_queue_get_first(task->queue, pkt, result);
			if (result == ERRTRYAGAIN){
				nanosleep((const struct timespec[]){{0, 100L}}, NULL);
			}
		}while(result == ERRTRYAGAIN);

		if (result == 0){
			sr_queue_remove(task->queue, pkt, result);
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


static void clean(void *p){
	if (p != NULL){
		Packet *pkt = (Packet*)p;
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

	sr_memory_default_init();

	int64_t start_time = sr_timing_start();

	Task plist[p_number];
	Task clist[c_number];


	for (i = 0; i < c_number; ++i){
		clist[i].id = i;
		clist[i].queue = &(clist[i].q);
		sr_queue_init(clist[i].queue, size);
		pthread_create(&(clist[i].consumers), NULL, consumers, &(clist[i]));
	}

	for (i = 0; i < p_number; ++i){
		plist[i].id = i;
		plist[i].queue = &(clist[i].q);
		pthread_create(&(plist[i].producer), NULL, producer, &(plist[i]));
	}


	for (i = 0; i < p_number; ++i){
		pthread_join(plist[i].producer, NULL);
	}


	for (i = 0; i < c_number; ++i){
		pthread_join(clist[i].consumers, NULL);
		clist[i].queue->clean = clean;
		sr_queue_clean(clist[i].queue);
	}


	logd("used time %ld\n", sr_timing_complete(start_time));

	sr_memory_debug(sr_log_info);

	sr_memory_release();

	return NULL;
}




int main(int argc, char *argv[])
{
	char *tmp = NULL;
	int result = 0;

	int64_t start_time = sr_timing_start();

	for (int i = 0; i < 10; ++i){
		malloc_test(10, 10);
		logi("malloc test ============================= %d\n", i);
	}

	logw("used time %lu %ld\n", malloc_count, sr_timing_complete(start_time));

	return 0;
}
