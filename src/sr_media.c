/*
 * sr_media.c
 *
 * Author: storyfrom1982@gmail.com
 *
 * Copyright (C) 2017 storyfrom1982@gmail.com all rights reserved.
 *
 * This file is part of sr_malloc.
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



#include "sr_media.h"


#include "sr_log.h"
#include "sr_byte.h"
#include "sr_error.h"
#include "sr_time.h"
#include "sr_pipe.h"
#include "sr_mutex.h"
#include "sr_malloc.h"

///////////////////////////////////////
//self-reliance media frame implement
///////////////////////////////////////


#define	FRAME_HEADER_SIZE		13

int sr_media_frame_create(Sr_media_frame **pp_frame)
{
	Sr_media_frame *frame = (Sr_media_frame *)malloc(sizeof(Sr_media_frame));

	if (frame == NULL){
		loge(ERRMALLOC);
		return ERRMALLOC;
	}

	memset(frame, 0, sizeof(Sr_media_frame));

	*pp_frame = frame;

	return 0;
}

void sr_media_frame_release(Sr_media_frame **pp_frame)
{
	if (pp_frame && *pp_frame){
		Sr_media_frame *frame = *pp_frame;
		*pp_frame = NULL;
		if (frame->frame_header){
			free(frame->frame_header);
		}
		free(frame);
	}
}

int sr_media_frame_fill_data(Sr_media_frame *frame, uint8_t *data, uint32_t size)
{
	uint8_t *p = NULL;

	if (frame == NULL || size == 0){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	frame->frame_size = 1 + 8 + 4 + size;
	frame->frame_header = (uint8_t *)malloc(frame->frame_size);
	if (frame->frame_header == NULL){
		loge(ERRMALLOC);
		return ERRMALLOC;
	}

	frame->data_size = size;
	frame->frame_data = frame->frame_header + FRAME_HEADER_SIZE;

	p = frame->frame_header;

	PUSHINT8(p, (frame->media_type & 0x07) << 5
			| (frame->data_type & 0x01) << 4
			| (frame->frame_flag & 0x0F));

	PUSHINT64(p, frame->timestamp);
	PUSHINT32(p, frame->data_size);

	if (data != NULL){
		memcpy(frame->frame_data, data, size);
	}

	return 0;
}

int sr_media_frame_fill_header(Sr_media_frame *frame)
{
	uint8_t *p = NULL;

	if (frame == NULL
			|| frame->frame_header == NULL
			|| frame->frame_size < FRAME_HEADER_SIZE){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	p = frame->frame_header;

	PUSHINT8(p, (frame->media_type & 0x07) << 5
			| (frame->data_type & 0x01) << 4
			| (frame->frame_flag & 0x0F));

	PUSHINT64(p, frame->timestamp);
	PUSHINT32(p, frame->data_size);

	return 0;
}

int sr_media_frame_parse_header(Sr_media_frame *frame)
{
	uint8_t type = 0;
	uint8_t *p = NULL;

	if (frame == NULL
			|| frame->frame_header == NULL
			|| frame->frame_size < 13){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	p = frame->frame_header;

	POPINT8(p, type);
	POPINT64(p, frame->timestamp);
	POPINT32(p, frame->data_size);

	if ((frame->frame_size - frame->data_size)
			!= FRAME_HEADER_SIZE){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	frame->media_type = (type >> 5) & 0x07;;
	frame->data_type = (type >> 4) & 0x01;
	frame->frame_flag = type & 0x0F;
	frame->frame_data = p;

	return 0;
}

///////////////////////////////////////
//self-reliance media format implement
///////////////////////////////////////

Sr_media_frame* sr_audio_config_to_media_frame(Sr_audio_config *config)
{
	int result = 0;
	uint32_t config_size = sizeof(Sr_audio_config);
	uint8_t config_data[config_size], *ptr = config_data;
	Sr_media_frame *frame = NULL;

	if ((result = sr_media_frame_create(&frame)) != 0){
		loge(result);
		return NULL;
	}

	frame->media_type = SR_FRAME_MEDIA_TYPE_AUDIO;
	frame->data_type = SR_FRAME_DATA_TYPE_MEDIA_CONFIG;
	frame->frame_flag = 0;
	frame->timestamp = 0;

	PUSHINT32(ptr, config->codec);
	PUSHINT32(ptr, config->bit_rate);
	PUSHINT32(ptr, config->channels);
	PUSHINT32(ptr, config->sample_rate);
	PUSHINT32(ptr, config->sample_type);
	PUSHINT32(ptr, config->bit_per_sample);
	PUSHINT32(ptr, config->byte_per_sample);
	PUSHINT32(ptr, config->sample_per_frame);
	PUSHINT8(ptr, config->extend_data_size);
	memcpy(ptr, config->extend_data, config->extend_data_size);

	result = sr_media_frame_fill_data(frame, config_data, config_size);
	if (result != 0){
		loge(result);
		return NULL;
	}

	return frame;
}

Sr_media_frame* sr_video_config_to_media_frame(Sr_video_config *config)
{
	int result = 0;
	uint32_t config_size = sizeof(Sr_video_config);
	uint8_t config_data[config_size], *ptr = config_data;
	Sr_media_frame *frame = NULL;

	if ((result = sr_media_frame_create(&frame)) != 0){
		loge(result);
		return NULL;
	}

	frame->media_type = SR_FRAME_MEDIA_TYPE_VIDEO;
	frame->data_type = SR_FRAME_DATA_TYPE_MEDIA_CONFIG;
	frame->frame_flag = SR_FRAME_VIDEO_FLAG_NONE;
	frame->timestamp = 0;

	PUSHINT32(ptr, config->codec);
	PUSHINT32(ptr, config->bit_rate);
	PUSHINT32(ptr, config->width);
	PUSHINT32(ptr, config->height);
	PUSHINT32(ptr, config->pixel_type);
	PUSHINT32(ptr, config->byte_per_pixel);
	PUSHINT32(ptr, config->frame_per_second);
	PUSHINT32(ptr, config->key_frame_interval);
	PUSHINT8(ptr, config->extend_data_size);
	memcpy(ptr, config->extend_data, config->extend_data_size);

	result = sr_media_frame_fill_data(frame, config_data, config_size);
	if (result != 0){
		loge(result);
		return NULL;
	}

	return frame;
}

Sr_audio_config* sr_media_frame_to_audio_config(Sr_media_frame *frame)
{
	uint8_t *ptr = NULL;
	Sr_audio_config *config = NULL;

	config = (Sr_audio_config *)malloc(sizeof(Sr_audio_config));
	if (config == NULL){
		loge(ERRMALLOC);
		return NULL;
	}

	ptr = frame->frame_data;

	POPINT32(ptr, config->codec);
	POPINT32(ptr, config->bit_rate);
	POPINT32(ptr, config->channels);
	POPINT32(ptr, config->sample_rate);
	POPINT32(ptr, config->sample_type);
	POPINT32(ptr, config->bit_per_sample);
	POPINT32(ptr, config->byte_per_sample);
	POPINT32(ptr, config->sample_per_frame);
	POPINT8(ptr, config->extend_data_size);
	memcpy(config->extend_data, ptr, config->extend_data_size);

	logd("audio channels = %d sample rate = %d sps size = %d\n",
			config->channels, config->sample_rate, config->extend_data_size);

	return config;
}

Sr_video_config* sr_media_frame_to_video_config(Sr_media_frame *frame)
{
	uint8_t *ptr = NULL;
	Sr_video_config *config = NULL;

	config = (Sr_video_config *)malloc(sizeof(Sr_video_config));
	if (config == NULL){
		loge(ERRMALLOC);
		return NULL;
	}

	ptr = frame->frame_data;

	POPINT32(ptr, config->codec);
	POPINT32(ptr, config->bit_rate);
	POPINT32(ptr, config->width);
	POPINT32(ptr, config->height);
	POPINT32(ptr, config->pixel_type);
	POPINT32(ptr, config->byte_per_pixel);
	POPINT32(ptr, config->frame_per_second);
	POPINT32(ptr, config->key_frame_interval);
	POPINT8(ptr, config->extend_data_size);
	memcpy(config->extend_data, ptr, config->extend_data_size);

	logd("video size = %dx%d sps size = %d\n",
			config->width, config->height, config->extend_data_size);

	return config;
}

///////////////////////////////////////
//self-reliance event listener implement
///////////////////////////////////////

#define EVENT_QUEUE_SIZE            1024

struct Sr_event_listener
{
	bool running;
	bool stopped;
	pthread_t tid;
	Sr_pipe *pipe;
	Sr_mutex *mutex;
	Sr_event_callback *cb;
};

static void *sr_event_listener_loop(void *p)
{
	logd("enter\n");

	int result = 0;
	Sr_event event = {0};
	unsigned int size = sizeof(event);
	Sr_event_listener *listener = (Sr_event_listener *) p;

	while (ISTRUE(listener->running)) {

		sr_mutex_lock(listener->mutex);

		if (sr_pipe_readable(listener->pipe) < size){
			sr_mutex_wait(listener->mutex);
			if (ISFALSE(listener->running)) {
				sr_mutex_unlock(listener->mutex);
				break;
			}
		}

		result = sr_pipe_read(listener->pipe, (uint8_t *) &event, size);

		sr_mutex_unlock(listener->mutex);

		if (result < 0) {
			loge(result);
			break;
		}

		listener->cb->notify(listener->cb, event);
	}

	SETTRUE(listener->stopped);

	logd("exit\n");

	return NULL;
}

int sr_event_listener_create(Sr_event_callback *cb, Sr_event_listener **pp_listener)
{
	logd("enter\n");

	int result = 0;
	Sr_event_listener *listener = NULL;

	if (pp_listener == NULL || cb == NULL || cb->notify == NULL) {
		loge(ERRPARAM);
		return ERRPARAM;
	}

	if ((listener = (Sr_event_listener *) calloc(1, sizeof(Sr_event_listener))) == NULL) {
		loge(ERRMALLOC);
		return ERRMALLOC;
	}

	listener->cb = cb;
	result = sr_pipe_create(sizeof(Sr_event) * EVENT_QUEUE_SIZE, &(listener->pipe));
	if (result != 0) {
		sr_event_listener_release(&listener);
		loge(result);
		return result;
	}

	result = sr_mutex_create(&(listener->mutex));
	if (result != 0) {
		sr_event_listener_release(&listener);
		loge(result);
		return result;
	}

	SETTRUE(listener->running);
	result = pthread_create(&(listener->tid), NULL, sr_event_listener_loop, listener);
	if (result != 0) {
		listener->tid = 0;
		sr_event_listener_release(&listener);
		loge(ERRSYSCALL);
		return ERRSYSCALL;
	}

	*pp_listener = listener;

	logd("exit\n");

	return 0;
}

void sr_event_listener_release(Sr_event_listener **pp_listener)
{
	logd("enter\n");

	if (pp_listener && *pp_listener) {
		Sr_event_listener *listener = *pp_listener;
		*pp_listener = NULL;
		if (listener->tid != 0){
			SETFALSE(listener->running);
			while(ISFALSE(listener->stopped)){
				sr_mutex_lock(listener->mutex);
				sr_mutex_broadcast(listener->mutex);
				sr_mutex_unlock(listener->mutex);
				nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
			}
			pthread_join(listener->tid, NULL);
		}
		sr_pipe_release(&(listener->pipe));
		sr_mutex_release(&(listener->mutex));
		free(listener);
	}

	logd("exit\n");
}

void sr_event_listener_push_event(Sr_event_listener *listener, Sr_event event)
{
	int result = 0;

	if (listener != NULL) {
		unsigned int size = sizeof(event);
		if (sr_pipe_writable(listener->pipe) >= size) {
			sr_mutex_lock(listener->mutex);
			if ((result = sr_pipe_write(listener->pipe, (uint8_t *) &event, size)) != size) {
				loge(result);
			}
			sr_mutex_signal(listener->mutex);
			sr_mutex_unlock(listener->mutex);
		}
	}
}

void sr_event_listener_push_type(Sr_event_listener *listener, int type)
{
	int result = 0;

	if (listener != NULL) {
		Sr_event event = { type, 0 };
		unsigned int size = sizeof(event);
		if (sr_pipe_writable(listener->pipe) >= size) {
			sr_mutex_lock(listener->mutex);
			if ((result = sr_pipe_write(listener->pipe, (uint8_t *) &event, size)) != size) {
				loge(result);
			}
			sr_mutex_signal(listener->mutex);
			sr_mutex_unlock(listener->mutex);
		}
	}
}

void sr_event_listener_push_error(Sr_event_listener *listener, int errorcode)
{
	int result = 0;

	if (listener != NULL) {
		Sr_event event = { .type = SR_EVENT_ERROR, .i32 = errorcode };
		unsigned int size = sizeof(event);
		if (sr_pipe_writable(listener->pipe) >= size) {
			sr_mutex_lock(listener->mutex);
			if ((result = sr_pipe_write(listener->pipe, (uint8_t *) &event, size)) != size) {
				loge(result);
			}
			sr_mutex_signal(listener->mutex);
			sr_mutex_unlock(listener->mutex);
		}
	}
}

///////////////////////////////////////
//self-reliance synchronous clock implement
///////////////////////////////////////

struct Sr_media_clock {

	bool fixed_audio;
	bool fixed_video;

	int64_t current;
	int64_t duration;
	int64_t audio_duration;
	int64_t first_audio_time;
	int64_t first_video_time;

	int64_t begin_time;

	Sr_mutex *mutex;
};

int sr_media_clock_create(Sr_media_clock **pp_clock)
{
	int result = 0;
	Sr_media_clock *sync_clock = (Sr_media_clock *)malloc(sizeof(Sr_media_clock));

	if (sync_clock == NULL){
		loge(ERRMALLOC);
		return ERRMALLOC;
	}

	if ((result = sr_mutex_create(&(sync_clock->mutex))) != 0){
		sr_media_clock_release(&(sync_clock));
		loge(result);
		return result;
	}

	sr_media_clock_reboot(sync_clock);

	*pp_clock = sync_clock;

	return 0;
}

void sr_media_clock_release(Sr_media_clock **pp_clock)
{
	if (pp_clock && *pp_clock){
		Sr_media_clock *sync_clock = *pp_clock;
		*pp_clock = NULL;
		sr_mutex_release(&(sync_clock->mutex));
		free(sync_clock);
	}
}

void sr_media_clock_reboot(Sr_media_clock *media_clock)
{
	if (media_clock){
		sr_mutex_lock(media_clock->mutex);
		media_clock->fixed_audio = false;
		media_clock->fixed_video = false;
		media_clock->current = 0;
		media_clock->duration = 0;
		media_clock->audio_duration = 0;
		media_clock->first_audio_time = 0;
		media_clock->first_video_time = 0;
		media_clock->begin_time = 0;
		sr_mutex_unlock(media_clock->mutex);
	}
}

int64_t sr_media_clock_get_duration(Sr_media_clock *media_clock)
{
	if (media_clock){
		return media_clock->audio_duration;
	}
	return 0;
}

int64_t sr_media_clock_update_audio_time(Sr_media_clock *media_clock,
											   int64_t microsecond)
{
	if (media_clock == NULL){
		loge(ERRPARAM);
		return 0;
	}

	sr_mutex_lock(media_clock->mutex);

	if (!media_clock->fixed_audio){
		media_clock->fixed_audio = true;
		media_clock->first_audio_time = microsecond;
		media_clock->audio_duration = microsecond;
		media_clock->current = microsecond;
		if (media_clock->begin_time == 0){
			media_clock->begin_time = sr_starting_time();
			media_clock->begin_time -= media_clock->first_audio_time;
		}else{
			if (media_clock->first_audio_time > media_clock->first_video_time){
				media_clock->begin_time -= (media_clock->first_audio_time - media_clock->first_video_time);
			}
		}
	}else{
		media_clock->audio_duration = microsecond;
		media_clock->current = sr_calculate_time(media_clock->begin_time);
	}

	media_clock->duration = media_clock->current;

	microsecond = media_clock->audio_duration - media_clock->duration;

	sr_mutex_unlock(media_clock->mutex);

	return microsecond;
}

int64_t sr_media_clock_update_video_time(Sr_media_clock *media_clock,
											   int64_t microsecond)
{
	if (media_clock == NULL){
		loge(ERRPARAM);
		return 0;
	}

	sr_mutex_lock(media_clock->mutex);

	if (!media_clock->fixed_video){
		media_clock->fixed_video = true;
		media_clock->first_video_time = microsecond;
		media_clock->current = microsecond;

		if (media_clock->begin_time == 0){
			media_clock->begin_time = sr_starting_time();
			media_clock->begin_time -= media_clock->first_video_time;
		}else{
			if (media_clock->first_audio_time < media_clock->first_video_time){
				media_clock->begin_time -= (media_clock->first_video_time - media_clock->first_audio_time);
			}
		}

	}else{
		media_clock->current = sr_calculate_time(media_clock->begin_time);
	}

	if (media_clock->audio_duration != 0){
		microsecond -= media_clock->audio_duration;
	}else{
		media_clock->duration = media_clock->current;
		microsecond -= media_clock->duration;
	}

	sr_mutex_unlock(media_clock->mutex);

	return microsecond;
}

///////////////////////////////////////
//self-reliance synchronous manager implement
///////////////////////////////////////

struct Sr_synchronous_manager{
	bool audio_rendering;
	bool video_rendering;
	Sr_synchronous_manager_callback *callback;
	Sr_media_transmission *transmission;
	Sr_media_clock *sync_clock;
	pthread_t audio_tid;
	pthread_t video_tid;
};

static void* audio_rendering_loop(void *p)
{
	int result = 0;
	int64_t delay = 0;
	int64_t begin_time = 0;
	int64_t used_time = 0;
	Sr_media_frame *frame = NULL;
	Sr_synchronous_manager *renderer = (Sr_synchronous_manager *)p;

	while(ISTRUE(renderer->audio_rendering)){

		begin_time = sr_starting_time();

		result = renderer->transmission->receive_frame(renderer->transmission, &frame, SR_FRAME_MEDIA_TYPE_AUDIO);
		if (result != 0){
			if (result == ERRTRYAGAIN){
				nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
				continue;
			}
			SETFALSE(renderer->audio_rendering);
			loge(result);
			break;
		}

		renderer->callback->render_audio(renderer->callback, frame);

		used_time = sr_calculate_time(begin_time);

		if (delay > 0){
			nanosleep((const struct timespec[]){{0, delay * 1000L}}, NULL);
		}

		if (frame->data_type != SR_FRAME_DATA_TYPE_MEDIA_CONFIG){
			delay = sr_media_clock_update_audio_time(renderer->sync_clock, frame->timestamp);
			delay -= used_time;
		}

		sr_media_frame_release(&frame);
	}

	return NULL;
}

static void* video_rendering_loop(void *p)
{
	int result = 0;
	int64_t delay = 0;
	int64_t begin_time = 0;
	int64_t used_time = 0;
	Sr_media_frame *frame = NULL;
	Sr_synchronous_manager *renderer = (Sr_synchronous_manager *)p;

	while(ISTRUE(renderer->video_rendering)){

		begin_time = sr_starting_time();

		result = renderer->transmission->receive_frame(renderer->transmission, &frame, SR_FRAME_MEDIA_TYPE_VIDEO);
		if (result != 0){
			if (result == ERRTRYAGAIN){
				nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
				continue;
			}
			SETFALSE(renderer->video_rendering);
			loge(result);
			break;
		}

		renderer->callback->render_video(renderer->callback, frame);

		used_time = sr_calculate_time(begin_time);

		if (delay > 0){
			nanosleep((const struct timespec[]){{0, delay * 1000L}}, NULL);
		}

		if (frame->data_type != SR_FRAME_DATA_TYPE_MEDIA_CONFIG){
			delay = sr_media_clock_update_video_time(renderer->sync_clock, frame->timestamp);
			delay -= used_time;
		}

		sr_media_frame_release(&frame);
	}

	return NULL;
}

int sr_synchronous_manager_create(Sr_media_transmission *transmission,
								  Sr_synchronous_manager_callback *callback,
								  Sr_synchronous_manager **pp_manager)
{
	int result = 0;
	Sr_synchronous_manager *manager = NULL;

	if (transmission == NULL
			|| callback == NULL
			|| callback->render_audio == NULL
			|| callback->render_video == NULL
			|| transmission->receive_frame == NULL
			|| pp_manager == NULL){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	manager = (Sr_synchronous_manager *)malloc(sizeof(Sr_synchronous_manager));
	if (manager == NULL){
		loge(ERRMALLOC);
		return ERRMALLOC;
	}
	memset(manager, 0, sizeof(Sr_synchronous_manager));

	manager->callback = callback;
	manager->transmission = transmission;

	result = sr_media_clock_create(&(manager->sync_clock));
	if (result != 0){
		sr_synchronous_manager_release(&manager);
		loge(result);
		return result;
	}

	manager->audio_rendering = true;
	result = pthread_create(&(manager->audio_tid), NULL, audio_rendering_loop, manager);
	if (result != 0){
		sr_synchronous_manager_release(&manager);
		loge(ERRSYSCALL);
		return ERRSYSCALL;
	}

	manager->video_rendering = true;
	result = pthread_create(&(manager->video_tid), NULL, video_rendering_loop, manager);
	if (result != 0){
		sr_synchronous_manager_release(&manager);
		loge(ERRSYSCALL);
		return ERRSYSCALL;
	}

	*pp_manager = manager;

	return 0;
}

void sr_synchronous_manager_release(Sr_synchronous_manager **pp_manager)
{
	if (pp_manager && *pp_manager){
		Sr_synchronous_manager *manager = *pp_manager;
		*pp_manager = NULL;
		if (manager->audio_tid != 0){
			SETFALSE(manager->audio_rendering);
			pthread_join(manager->audio_tid, NULL);
		}
		if (manager->video_tid != 0){
			SETFALSE(manager->video_rendering);
			pthread_join(manager->video_tid, NULL);
		}
		sr_media_clock_release(&(manager->sync_clock));
		free(manager);
	}
}
