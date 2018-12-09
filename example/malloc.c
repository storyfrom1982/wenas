/*
 * malloc.c
 *
 *  Created on: 2017年3月1日
 *      Author: kly
 */


#include <sr_lib.h>
#include <sr_malloc.h>


pthread_key_t key;
void *ppp = &key;


typedef struct packet_node_t{
	sr_node_t *prev;
	sr_node_t *next;
	int id;
	size_t size;
	uint8_t *data;
}packet_node_t;

//#define BLOCKING

typedef struct task_t{
	int id;
	pthread_t producer;
	pthread_t consumers;
	sr_queue_t *queue;
}task_t;


static uint64_t malloc_count = 0;
static uint64_t free_count = 0;


static void free_node_callback(sr_node_t *node)
{
	packet_node_t *pkt = (packet_node_t *)node;
	if (pkt != NULL){
		if (pkt->data){
//			logd("%s\n", pkt->data);
			free(pkt->data);
			__sr_atom_add(free_count, 1);
		}
		free(pkt);
		__sr_atom_add(free_count, 1);
	}else{
		logd("free_node_callback pkt == NULL\n");
	}
}


static void* producer(void *p)
{
	if (pthread_setspecific(key, ppp) != 0){
		return NULL;
	}

	int result = 0;

	void *add = malloc(1024 * 1024 * 4);
	if (add == NULL){
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
		pkt->size = random() % 102400;
		if (pkt->size < 128){
			pkt->size = 128;
		}
		pkt->data = (uint8_t*)malloc(pkt->size);
//		pkt->data = (uint8_t*)calloc(1, pkt->size);
//		pkt->data = (uint8_t*)sr_memory_calloc(1, pkt->size, NULL);

		if (pkt->data == NULL){
			loge("malloc failed\n");
			exit(0);
			break;
		}
		__sr_atom_add(malloc_count, 1);
		snprintf(pkt->data, pkt->size, "producer id = %d malloc size = %lu iiiid===========: %d",
				task->id, pkt->size, i + task->id * 10);
//		pkt->data = (uint8_t*)realloc(pkt->data, pkt->size + 1024);


#ifdef BLOCKING
		result = __sr_queue_block_push_back(task->queue, pkt);
#else
		while((result = __sr_queue_push_back(task->queue, pkt)) == QUEUE_RESULT_TRY_AGAIN) {
			nanosleep((const struct timespec[]){{0, 100L}}, NULL);
		};
#endif

		if (result != QUEUE_RESULT_OK){
			logd("%s\n", pkt->data);
			free(pkt->data);
			free(pkt);
			pkt = NULL;
			__sr_atom_add(free_count, 2);
			break;
		}
	}

	logd("tid %d sr_queue_stop enter queue stopped: %d queue node count: %d\n", task->id,
			sr_queue_is_stopped(task->queue), sr_queue_popable(task->queue));
//	sr_queue_stop(task->queue);
	sr_queue_finish(task->queue);
	logd("tid %d sr_queue_stop exit queue stopped: %d queue node count: %d\n", task->id,
			sr_queue_is_stopped(task->queue), sr_queue_popable(task->queue));

	free(add);

	return NULL;
}

static void* consumers(void *p)
{
	if (pthread_setspecific(key, ppp) != 0){
		return NULL;
	}

	int result = 0;

	packet_node_t *pkt = NULL;
	task_t *task = (task_t*)p;

	while(true){


#ifdef BLOCKING
		result = __sr_queue_block_pop_front(task->queue, pkt);
#else
		while((result = __sr_queue_pop_front(task->queue, pkt)) == QUEUE_RESULT_TRY_AGAIN) {
			nanosleep((const struct timespec[]){{0, 100L}}, NULL);
		}
#endif

		if (result == 0){
//			logd("%s\n", pkt->data);
			if (pkt != NULL){
				if (pkt->data != NULL){
					free(pkt->data);
				}
				free(pkt);
			}
			pkt = NULL;
			__sr_atom_add(free_count, 2);
		}else{
			logd("tid %d exit =========================  queue node count %d\n",
					task->id, sr_queue_popable(task->queue));
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

	if (pthread_key_create(&(key), NULL) != 0){
		return NULL;
	}

	if (pthread_setspecific(key, ppp) != 0){
		return NULL;
	}

//	sr_memory_default_init();

	int64_t start_time = sr_time_begin();

	task_t plist[p_number];
	task_t clist[c_number];

	for (i = 0; i < c_number; ++i){
		clist[i].id = i;
		clist[i].queue = sr_queue_create(size, free_node_callback);
		pthread_create(&(clist[i].consumers), NULL, consumers, &(clist[i]));
	}

	for (i = 0; i < p_number; ++i){
		plist[i].id = i;
		if (i < c_number){
			plist[i].queue = clist[i].queue;
		}else{
			plist[i].queue = clist[i % (c_number)].queue;
		}

		pthread_create(&(plist[i].producer), NULL, producer, &(plist[i]));
	}


	for (i = 0; i < p_number; ++i){
//		logd("producer ===================================== %d enter\n", plist[i].id);
		pthread_join(plist[i].producer, NULL);
//		sr_queue_stop(plist[i].queue);
//		logd("producer ===================================== %d exit\n", plist[i].id);
	}


	for (i = 0; i < c_number; ++i){
//		logd("consumers ===================================== %d enter\n", clist[i].id);
//		sr_queue_stop(clist[i].queue);
		pthread_join(clist[i].consumers, NULL);
		sr_queue_release(&clist[i].queue);
//		logd("consumers ===================================== %d exit\n", clist[i].id);
	}

//	int release_number = p_number;
//	Task *release_list = plist;
//	if (c_number < p_number){
//		release_number = c_number;
//		release_list = clist;
//	}
//
//	for (i = 0; i < release_number; ++i){
//		sr_queue_release(&release_list[i].queue);
//	}


	logd("used time %ld\n", sr_time_passed(start_time));

//	sr_memory_debug(sr_log_info);

//	sr_memory_release();

	return NULL;
}


int main(int argc, char *argv[])
{
	sr_malloc_initialize(1024 * 1024 * 10, 4);

	char *tmp = NULL;
	int result = 0;

	int64_t start_time = sr_time_begin();

	for (int i = 0; i < 10; ++i){
		malloc_test(20, 20);
		logd("malloc test ============================= %d\n", i);
	}

	logd("malloc count: %lu  free count: %lld used time: %ld\n", malloc_count, free_count, sr_time_passed(start_time));

	for (int i = 0; i < 1; ++i){
		malloc_test(40, 10);
		logd("malloc test ============================= %d\n", i);
		logd("malloc count: %lu  free count: %lld used time: %ld\n", malloc_count, free_count, sr_time_passed(start_time));
	}

	sr_malloc_debug(sr_log_warn);

	sr_malloc_release();

	logd("malloc count: %lu  free count: %lld used time: %ld\n", malloc_count, free_count, sr_time_passed(start_time));

	return 0;
}
