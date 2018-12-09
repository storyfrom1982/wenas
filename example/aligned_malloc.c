/*
 * malloc.c
 *
 *  Created on: 2017年3月1日
 *      Author: kly
 */


#include <sr_lib.h>
#include <sr_malloc.h>



typedef struct packet_node_t{
	sr_node_t *prev;
	sr_node_t *next;
	int id;
	size_t size;
	uint8_t *data;
}packet_node_t;


typedef struct task_t{
	int id;
	pthread_t producer;
	pthread_t consumers;
	sr_queue_t *queue;
}task_t;


static uint64_t malloc_count = 0;
static uint64_t free_count = 0;


static void free_node(sr_node_t *p){
	if (p != NULL){
		packet_node_t *pkt = (packet_node_t*)p;
//		logd("%s\n", pkt->data);
		free(pkt->data);
		free(pkt);
		__sr_atom_add(free_count, 2);
	}
}


static void* producer(void *p)
{
	int result = 0;
	size_t aligned = sizeof(size_t);
	aligned = 1024;

	void *add = aligned_alloc(aligned, 1024 * 1024 * 4);
	if (add == NULL){
		loge("aligned_alloc failed\n");
		exit(0);
		return NULL;
	}

	packet_node_t *pkt = NULL;
	task_t *task = (task_t*)p;

	for(int i = 0; i < 100000; ++i){
		pkt = (packet_node_t*)malloc(sizeof(packet_node_t));
		if (pkt == NULL){
			loge("malloc failed\n");
			exit(0);
			break;
		}
		__sr_atom_add(malloc_count, 1);
		pkt->id = task->id;
		pkt->size = random() % 10240;
		if (pkt->size < 128){
			pkt->size = 128;
		}
//		pkt->size = pkt->size + aligned & ~(aligned-1);
		pkt->data = (uint8_t*)aligned_alloc(aligned, pkt->size);
//		pkt->data = (uint8_t*)calloc(1, pkt->size);
		if (pkt->data == NULL){
			loge("aligned_alloc failed\n");
			exit(0);
			break;
		}
		__sr_atom_add(malloc_count, 1);
//		snprintf(pkt->data, pkt->size, "producer id = %d malloc size = %lu", task->id, pkt->size);
//		pkt->data = (uint8_t*)realloc(pkt->data, pkt->size + 1);

		while((result = __sr_queue_push_back(task->queue, pkt)) == QUEUE_RESULT_TRY_AGAIN) {
			nanosleep((const struct timespec[]){{0, 100L}}, NULL);
		}

		if (result != 0){
			free(pkt->data);
			free(pkt);
		}
	}


//	sr_queue_stop(task->queue);
	sr_queue_finish(task->queue);

	free(add);

	pthread_exit(0);

	return NULL;
}

static void* consumers(void *p)
{
	int result = 0;

	packet_node_t *pkt = NULL;
	task_t *task = (task_t*)p;

	while(true){

		while((result = __sr_queue_pop_front(task->queue, pkt)) == QUEUE_RESULT_TRY_AGAIN) {
			nanosleep((const struct timespec[]){{0, 100L}}, NULL);
		}

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

	int64_t start_time, passed_time;
	__sr_time_begin(start_time);

	task_t plist[p_number];
	task_t clist[c_number];

	for (i = 0; i < c_number; ++i){
		clist[i].id = i;
		clist[i].queue = sr_queue_create(size, free_node);
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
	}

	__sr_time_passed(start_time, passed_time);
	logd("used time %ld\n", passed_time);

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

	int64_t start_time = sr_time_begin();

	for (int i = 0; i < 10; ++i){
		malloc_test(2, 2);
		logd("malloc test ============================= %d\n", i);
	}

	sr_malloc_debug(sr_log_warn);

	pthread_exit(0);

	sr_malloc_release();

	loge("used time %lu %ld\n", malloc_count, sr_time_passed(start_time));

	return 0;
}
