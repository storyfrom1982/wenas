/*
 * memory_leak.c
 *
 *  Created on: 2017年3月1日
 *      Author: kly
 */

#include <sr_lib.h>
#include <sr_malloc.h>

//#define BLOCKING

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

	void *addres = malloc(1024 * 1024 * 4);
	if (addres == NULL){
		loge("malloc failed\n");
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
		pkt->data = (uint8_t*)malloc(pkt->size);
//		pkt->data = (uint8_t*)calloc(1, pkt->size);
		if (pkt->data == NULL){
			loge("malloc failed\n");
			exit(0);
			break;
		}
		__sr_atom_add(malloc_count, 1);
		snprintf(pkt->data, pkt->size, "producer id = %d malloc size = %lu", task->id, pkt->size);
//		pkt->data = (uint8_t*)realloc(pkt->data, pkt->size + 1);

		while((result = __sr_queue_push_back(task->queue, pkt)) == QUEUE_RESULT_TRY_AGAIN) {
			nanosleep((const struct timespec[]){{0, 100L}}, NULL);
		}

		if (result != 0){
			loge("sr_queue_push_back failed\n");
			free(pkt->data);
			free(pkt);
			break;
		}
	}


	logd("sr_queue_stop enter\n");
	sr_queue_stop(task->queue);
	logd("sr_queue_stop exit\n");

	free(addres);

	return NULL;
}

static void* consumers(void *p)
{
	int result = 0;

	packet_node_t *pkt = NULL;
	task_t *task = (task_t*)p;

	while(true){

#ifdef BLOCKING
		sr_queue_block_clean(task->queue);
		nanosleep((const struct timespec[]){{0, 1000000L}}, NULL);
#else
		sr_queue_lock(task->queue);
		sr_node_t *head = sr_queue_get_first(task->queue);
		while(head->next != NULL) {
			result = __sr_queue_remove_node(task->queue, head);
			free_node(head);
			head = sr_queue_get_first(task->queue);
		}
		sr_queue_unlock(task->queue);
#endif

		if (sr_queue_is_stopped(task->queue)){
			logd("consumers stopped\n");
			break;
		}
	}

	return NULL;
}


void* malloc_test(int producer_count, int consumers_count)
{
	int i = 0;
	int size = 1024000;
	int result = 0;
	int c_number = consumers_count;
	int p_number = producer_count;

	int64_t start_time = sr_time_begin();

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
//		sr_queue_release(&clist[i].queue);
	}


	logd("used time %ld\n", sr_time_passed(start_time));

	sr_malloc_debug(sr_log_warn);

	sr_malloc_release();


	return NULL;
}




int main(int argc, char *argv[])
{
	sr_malloc_initialize(1024 * 1024 * 8, 2);

	char *tmp = NULL;
	int result = 0;

	int64_t start_time = sr_time_begin();

	for (int i = 0; i < 1; ++i){
		malloc_test(10, 10);
		logd("malloc test ============================= %d\n", i);
	}

	logw("malloc count: %lu free count: %lu used time: %ld\n", malloc_count, free_count, sr_time_passed(start_time));

	sr_malloc_release();

	return 0;
}
