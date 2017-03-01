/*
 * sr_media_file.c
 *
 * Author: storyfrom1982@gmail.com
 *
 * Copyright (C) 2017 storyfrom1982@gmail.com all rights reserved.
 *
 * This file is part of self-reliance.
 *
 * self-reliance is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * self-reliance is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */



#include "sr_media_file.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <pthread.h>

#include "sr_log.h"
#include "sr_atom.h"
#include "sr_byte.h"
#include "sr_error.h"
#include "sr_time.h"
#include "sr_pipe.h"
#include "sr_mutex.h"
#include "sr_memory.h"
#include "sr_queue.h"


typedef struct sr_media_transmission_protocol_t{

	int fd;
	char *path;

	bool running;
	pthread_t tid;

	SR_MediaTransmission transmission;
	SR_EventListener *listener;

	SR_QUEUE_DECLARE(SR_MediaFrame) sender_queue;
	SR_QUEUE_DECLARE(SR_MediaFrame) receiver_audio_queue;
	SR_QUEUE_DECLARE(SR_MediaFrame) receiver_video_queue;

}sr_file_protocol_t;



///////////////////////////////////////
//write
///////////////////////////////////////


static void sr___file_protocol___writer_flush(SR_MediaTransmission *transmission)
{
	if (transmission && transmission->protocol){
		while(ISTRUE(transmission->protocol->running)
				&& sr_queue_popable(&(transmission->protocol->sender_queue)) > 0){
			nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
		}
	}
}


static int sr___file_protocol___writer_disable_read(
		SR_MediaTransmission *transmission,
		SR_MediaFrame **pp_frame, int type)
{
	return ERRCANCEL;
}


static int sr___file_protocol___write_frame(SR_MediaTransmission *sender, SR_MediaFrame *frame)
{
	int result = 0;

	if (sender == NULL || sender->protocol == NULL || frame == NULL){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	if (ISFALSE(sender->protocol->running)){
		return ERRCANCEL;
	}

	sr_queue_push_to_end(&sender->protocol->sender_queue, frame, result);

	return result;
}


static void* write_loop(void *p)
{
	int result = 0;
	int64_t audio_timestamp;
	int64_t video_timestamp;
	int64_t audio_begin_time = 0;
	int64_t video_begin_time = 0;
	bool running = false;
	SR_MediaFrame *frame = NULL;
	sr_file_protocol_t *fp = (sr_file_protocol_t *)p;


	while(ISTRUE(fp->running)){

		sr_queue_pop_first(&(fp->sender_queue), frame, result);

		if (result != 0){
			if (result == ERRTRYAGAIN){
				nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
				continue;
			}else{
				loge(result);
				break;
			}
		}

		if (frame->media_type == SR_FRAME_MEDIA_TYPE_AUDIO){
//			if (audio_begin_time == 0){
//				audio_timestamp = 0;
//				audio_begin_time = sr___timing___start();
//			}else{
//				audio_timestamp = sr___timing___complete(audio_begin_time);
//			}
//			frame->timestamp = audio_timestamp;
			logw("audio pts ============ %lld\n", frame->timestamp);
		}else if (frame->media_type == SR_FRAME_MEDIA_TYPE_VIDEO){
//			if (video_begin_time == 0){
//				video_timestamp = 0;
//				video_begin_time = sr___timing___start();
//			}else{
//				video_timestamp = sr___timing___complete(video_begin_time);
//			}
//			frame->timestamp = video_timestamp;
			logw("video pts ============ %lld\n", frame->timestamp);
		}


		for (int size = 0; size < frame->frame_size; size += result){
			result = write(fp->fd, frame->frame_header + size, frame->frame_size - size);
			if (result <= 0){
				if (errno != EAGAIN){
					result = ERRSYSCALL;
					break;
				}
				result = 0;
			}
		}

		sr_media_frame_release(&frame);


		if (SETTRUE(running)){
			sr_event_listener_push_type(fp->listener, SR_EVENT_RUNNING);
		}

	}


	if (ISFALSE(fp->running)){
		result = ERRCANCEL;
	}

	SR_Event event = {.type = SR_EVENT_TERMINATION, .i32 = result};
	sr_event_listener_push_event(fp->listener, event);


	return NULL;
}


///////////////////////////////////////
//read
///////////////////////////////////////


static void sr___file_protocol___reader_flush(SR_MediaTransmission *transmission)
{
	if (transmission){
		while(ISTRUE(transmission->protocol->running) &&
				(sr_queue_popable(&(transmission->protocol->receiver_audio_queue)) > 0
						|| sr_queue_popable(&(transmission->protocol->receiver_video_queue)) > 0)){
			nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
		}
	}
}


static int sr___file_protocol___reader_disable_write(
		SR_MediaTransmission *transmission,
		SR_MediaFrame *frame)
{
	return ERRCANCEL;
}


static int sr___file_protocol___read_frame(SR_MediaTransmission *receiver, SR_MediaFrame **pp_frame, int type)
{
	int result = 0;
	SR_MediaFrame *frame = NULL;

	if (receiver == NULL || receiver->protocol == NULL || pp_frame == NULL){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	if (ISTRUE(receiver->protocol->running)){

		if (type == SR_FRAME_MEDIA_TYPE_AUDIO){
			sr_queue_pop_first(&(receiver->protocol->receiver_audio_queue), frame, result);
		}else if (type == SR_FRAME_MEDIA_TYPE_VIDEO){
			sr_queue_pop_first(&(receiver->protocol->receiver_video_queue), frame, result);
		}else{
			result = ERRPARAM;
		}

		if (result == 0){
			*pp_frame = frame;
		}

	}else{
		result = ERRCANCEL;
	}

	return result;
}


static void* read_loop(void *p)
{
	int result = 0;
	bool running = false;
	SR_MediaFrame *frame = NULL;
	sr_file_protocol_t *fp = (sr_file_protocol_t *)p;

	uint8_t type = 0;
	uint8_t *ptr = NULL;
	uint8_t head[13] = {0};

	int64_t delay = 0;
	int64_t begin_time = 0;
	int64_t used_time = 0;


	while(ISTRUE(fp->running)){

		if ((result = sr_media_frame_create(&frame)) != 0){
			loge(result);
			break;
		}

		for (ssize_t size = 0; size < 13; size += result){
			result = read(fp->fd, head + size, 13 - size);
			if (result <= 0){
				if (errno != EAGAIN){
					result = ERRSYSCALL;
					break;
				}
				result = 0;
			}
		}

		if (result < 0){
			loge(result);
			break;
		}

		ptr = head;
		POPINT8(ptr, type);
		POPINT64(ptr, frame->timestamp);
		POPINT32(ptr, frame->data_size);
		frame->media_type = (type >> 5) & 0x07;
		frame->data_type = (type >> 4) & 0x01;
		frame->frame_flag = type & 0x0F;

		result = sr_media_frame_fill_data(frame, NULL, frame->data_size);
		if (result != 0){
			loge(result);
			break;
		}

		for (ssize_t size = 0; size < frame->data_size; size += result){
			result = read(fp->fd, frame->frame_data + size, frame->data_size - size);
			if (result <= 0){
				if (errno != EAGAIN){
					result = ERRSYSCALL;
					break;
				}
				result = 0;
			}
		}

		if (result < 0){
			loge(result);
			break;
		}

		if (frame->media_type == SR_FRAME_MEDIA_TYPE_AUDIO){
			logw("read audio == %u\n", frame->data_size);
			do {
				sr_queue_push_to_end(&(fp->receiver_audio_queue), frame, result);
				if (result == ERRTRYAGAIN){
					if (ISFALSE(fp->running)){
						break;
					}
					nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
				}
			}while(result == ERRTRYAGAIN);

		}else if (frame->media_type == SR_FRAME_MEDIA_TYPE_VIDEO){
			logw("read video == %u\n", frame->data_size);
			do {
				sr_queue_push_to_end(&(fp->receiver_video_queue), frame, result);
				if (result == ERRTRYAGAIN){
					if (ISFALSE(fp->running)){
						break;
					}
					nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
				}
			}while(result == ERRTRYAGAIN);

		}else{

			sr_media_frame_release(&frame);
		}


		if (result != 0){
			loge(result);
			break;
		}


		if (SETTRUE(running)){
			sr_event_listener_push_type(fp->listener, SR_EVENT_RUNNING);
		}

	}

	if (frame != NULL){
		sr_media_frame_release(&frame);
	}

	while(ISTRUE(fp->running) &&
			(sr_queue_popable(&(fp->receiver_audio_queue)) > 0
					|| sr_queue_popable(&(fp->receiver_video_queue)) > 0)){
		nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
	}

	SR_Event event = {.type = SR_EVENT_TERMINATION, .i32 = result};
	sr_event_listener_push_event(fp->listener, event);


	return NULL;
}


///////////////////////////////////////
//open/close
///////////////////////////////////////


static void file_protocol_release(sr_file_protocol_t *fp)
{
	SETFALSE(fp->running);
	if (fp->tid != 0){
		pthread_join(fp->tid, NULL);
	}
	sr_queue_clean(&(fp->sender_queue));
	sr_queue_clean(&(fp->receiver_audio_queue));
	sr_queue_clean(&(fp->receiver_video_queue));
	if (fp->fd > 2){
		close(fp->fd);
	}
	free(fp->path);
	free(fp);
}


int sr_file_protocol_create_writer(const char *path,
		SR_EventListener *listener,
		SR_MediaTransmission **pp_transmission)
{
	int result = 0;
	uint32_t queue_size = 1024;
	sr_file_protocol_t *fp = NULL;

	if (path == NULL || listener == NULL || pp_transmission == NULL){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	logd("create file %s\n", path);

	if ((fp = (sr_file_protocol_t *)calloc(1, sizeof(sr_file_protocol_t))) == NULL){
		loge(ERRMEMORY);
		return ERRMEMORY;
	}

	fp->listener = listener;

	sr_event_listener_push_type(fp->listener, SR_EVENT_INITIALIZE);

	if ((fp->path = strdup(path)) == NULL){
		file_protocol_release(fp);
		loge(ERRMEMORY);
		return ERRMEMORY;
	}

	if ((fp->fd = open(fp->path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0){
		file_protocol_release(fp);
		loge(ERRSYSCALL);
		return ERRSYSCALL;
	}

	sr_queue_init(&(fp->sender_queue), queue_size);

	fp->running = true;

	if ((result = pthread_create(&(fp->tid), NULL, write_loop, fp)) != 0){
		file_protocol_release(fp);
		loge(ERRSYSCALL);
		return ERRSYSCALL;
	}

	fp->transmission.protocol = fp;
	fp->transmission.send_frame = sr___file_protocol___write_frame;
	fp->transmission.receive_frame = sr___file_protocol___writer_disable_read;
	fp->transmission.flush = sr___file_protocol___writer_flush;

	*pp_transmission = &(fp->transmission);

	logd("exit\n");

	return 0;
}


int sr_file_protocol_create_reader(const char *path,
		SR_EventListener *listener,
		SR_MediaTransmission **pp_transmission)
{
	int result = 0;
	int queue_size = 1024;
	sr_file_protocol_t *fp = NULL;

	if (path == NULL || listener == NULL || pp_transmission == NULL){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	logd("open file %s\n", path);

	if ((fp = (sr_file_protocol_t *)calloc(1, sizeof(sr_file_protocol_t))) == NULL){
		loge(ERRMEMORY);
		return ERRMEMORY;
	}

	fp->listener = listener;

	sr_event_listener_push_type(fp->listener, SR_EVENT_INITIALIZE);

	if ((fp->path = strdup(path)) == NULL){
		file_protocol_release(fp);
		loge(ERRMEMORY);
		return ERRMEMORY;
	}

	if ((fp->fd = open(fp->path, O_RDONLY, 0644)) < 0){
		file_protocol_release(fp);
		loge(ERRSYSCALL);
		return ERRSYSCALL;
	}

	sr_queue_init(&(fp->receiver_audio_queue), queue_size);
	sr_queue_init(&(fp->receiver_video_queue), queue_size);

	fp->running = true;

	if ((result = pthread_create(&(fp->tid), NULL, read_loop, fp)) != 0){
		file_protocol_release(fp);
		loge(ERRSYSCALL);
		return ERRSYSCALL;
	}

	fp->transmission.protocol = fp;
	fp->transmission.send_frame = sr___file_protocol___reader_disable_write;
	fp->transmission.receive_frame = sr___file_protocol___read_frame;
	fp->transmission.flush = sr___file_protocol___reader_flush;

	*pp_transmission = &(fp->transmission);

	logd("exit\n");

	return 0;
}


void sr_file_protocol_release(SR_MediaTransmission **pp_transmission)
{
	logd("enter\n");
	if (pp_transmission && *pp_transmission){
		SR_MediaTransmission *transmission = *pp_transmission;
		*pp_transmission = NULL;
		file_protocol_release(transmission->protocol);
	}
	logd("exit\n");
}


void sr_file_protocol_stop(SR_MediaTransmission *transmission)
{
	if (transmission && transmission->protocol){
		SETFALSE(transmission->protocol->running);
	}
}
