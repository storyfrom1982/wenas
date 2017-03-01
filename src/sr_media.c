/*
 * sr_media.c
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



#include "sr_media.h"


#include "sr_log.h"
#include "sr_byte.h"
#include "sr_error.h"
#include "sr_time.h"
#include "sr_pipe.h"
#include "sr_mutex.h"
#include "sr_memory.h"

///////////////////////////////////////
//self-reliance media frame implement
///////////////////////////////////////


#define	FRAME_HEADER_SIZE		13

int sr_media_frame_create(SR_MediaFrame **pp_frame)
{
	SR_MediaFrame *frame = (SR_MediaFrame *)malloc(sizeof(SR_MediaFrame));

	if (frame == NULL){
		loge(ERRMEMORY);
		return ERRMEMORY;
	}

	memset(frame, 0, sizeof(SR_MediaFrame));

	*pp_frame = frame;

	return 0;
}

void sr_media_frame_release(SR_MediaFrame **pp_frame)
{
	if (pp_frame && *pp_frame){
		SR_MediaFrame *frame = *pp_frame;
		*pp_frame = NULL;
		if (frame->frame_header){
			free(frame->frame_header);
		}
		free(frame);
	}
}

int sr_media_frame_fill_data(SR_MediaFrame *frame, uint8_t *data, uint32_t size)
{
	uint8_t *p = NULL;

	if (frame == NULL || size == 0){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	frame->frame_size = 1 + 8 + 4 + size;
	frame->frame_header = (uint8_t *)malloc(frame->frame_size);
	if (frame->frame_header == NULL){
		loge(ERRMEMORY);
		return ERRMEMORY;
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

int sr_media_frame_fill_header(SR_MediaFrame *frame)
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

int sr_media_frame_parse_header(SR_MediaFrame *frame)
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

SR_MediaFrame* sr_audio_format_to_media_frame(SR_AudioFormat *format)
{
	int result = 0;
	uint32_t format_size = sizeof(SR_AudioFormat);
	uint8_t format_data[format_size], *ptr = format_data;
	SR_MediaFrame *frame = NULL;

	if ((result = sr_media_frame_create(&frame)) != 0){
		loge(result);
		return NULL;
	}

	frame->media_type = SR_FRAME_MEDIA_TYPE_AUDIO;
	frame->data_type = SR_FRAME_DATA_TYPE_MEDIA_FORMAT;
	frame->frame_flag = 0;
	frame->timestamp = 0;

	PUSHINT32(ptr, format->codec_type);
	PUSHINT32(ptr, format->bit_rate);
	PUSHINT32(ptr, format->channels);
	PUSHINT32(ptr, format->sample_rate);
	PUSHINT32(ptr, format->sample_type);
	PUSHINT32(ptr, format->bit_per_sample);
	PUSHINT32(ptr, format->byte_per_sample);
	PUSHINT32(ptr, format->sample_per_frame);
	PUSHINT8(ptr, format->extend_data_size);
	memcpy(ptr, format->extend_data, format->extend_data_size);

	result = sr_media_frame_fill_data(frame, format_data, format_size);
	if (result != 0){
		loge(result);
		return NULL;
	}

	return frame;
}

SR_MediaFrame* sr_video_format_to_media_frame(SR_VideoFormat *format)
{
	int result = 0;
	uint32_t format_size = sizeof(SR_VideoFormat);
	uint8_t format_data[format_size], *ptr = format_data;
	SR_MediaFrame *frame = NULL;

	if ((result = sr_media_frame_create(&frame)) != 0){
		loge(result);
		return NULL;
	}

	frame->media_type = SR_FRAME_MEDIA_TYPE_VIDEO;
	frame->data_type = SR_FRAME_DATA_TYPE_MEDIA_FORMAT;
	frame->frame_flag = SR_FRAME_VIDEO_FLAG_NONE;
	frame->timestamp = 0;

	PUSHINT32(ptr, format->codec_type);
	PUSHINT32(ptr, format->bit_rate);
	PUSHINT32(ptr, format->width);
	PUSHINT32(ptr, format->height);
	PUSHINT32(ptr, format->pixel_type);
	PUSHINT32(ptr, format->byte_per_pixel);
	PUSHINT32(ptr, format->frame_per_second);
	PUSHINT32(ptr, format->key_frame_interval);
	PUSHINT8(ptr, format->extend_data_size);
	memcpy(ptr, format->extend_data, format->extend_data_size);

	result = sr_media_frame_fill_data(frame, format_data, format_size);
	if (result != 0){
		loge(result);
		return NULL;
	}

	return frame;
}

SR_AudioFormat* sr_media_frame_to_audio_format(SR_MediaFrame *frame)
{
	uint8_t *ptr = NULL;
	SR_AudioFormat *format = NULL;

	format = (SR_AudioFormat *)malloc(sizeof(SR_AudioFormat));
	if (format == NULL){
		loge(ERRMEMORY);
		return NULL;
	}

	ptr = frame->frame_data;

	POPINT32(ptr, format->codec_type);
	POPINT32(ptr, format->bit_rate);
	POPINT32(ptr, format->channels);
	POPINT32(ptr, format->sample_rate);
	POPINT32(ptr, format->sample_type);
	POPINT32(ptr, format->bit_per_sample);
	POPINT32(ptr, format->byte_per_sample);
	POPINT32(ptr, format->sample_per_frame);
	POPINT8(ptr, format->extend_data_size);
	memcpy(format->extend_data, ptr, format->extend_data_size);

	logd("audio channels = %d sample rate = %d sps size = %d\n",
			format->channels, format->sample_rate, format->extend_data_size);

	return format;
}

SR_VideoFormat* sr_media_frame_to_video_format(SR_MediaFrame *frame)
{
	uint8_t *ptr = NULL;
	SR_VideoFormat *format = NULL;

	format = (SR_VideoFormat *)malloc(sizeof(SR_VideoFormat));
	if (format == NULL){
		loge(ERRMEMORY);
		return NULL;
	}

	ptr = frame->frame_data;

	POPINT32(ptr, format->codec_type);
	POPINT32(ptr, format->bit_rate);
	POPINT32(ptr, format->width);
	POPINT32(ptr, format->height);
	POPINT32(ptr, format->pixel_type);
	POPINT32(ptr, format->byte_per_pixel);
	POPINT32(ptr, format->frame_per_second);
	POPINT32(ptr, format->key_frame_interval);
	POPINT8(ptr, format->extend_data_size);
	memcpy(format->extend_data, ptr, format->extend_data_size);

	logd("video size = %dx%d sps size = %d\n",
			format->width, format->height, format->extend_data_size);

	return format;
}

///////////////////////////////////////
//self-reliance event listener implement
///////////////////////////////////////

#define EVENT_QUEUE_SIZE            1024

struct SR_EventListener
{
	bool running;
	bool stopped;
	pthread_t tid;
	SR_Pipe *pipe;
	SR_Mutex *mutex;
	SR_Event_Callback *cb;
};

static void *sr___event_listener___loop(void *p)
{
	logd("enter\n");

	int result = 0;
	SR_Event event = {0};
	unsigned int size = sizeof(event);
	SR_EventListener *listener = (SR_EventListener *) p;

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

int sr_event_listener_create(SR_Event_Callback *cb, SR_EventListener **pp_listener)
{
	logd("enter\n");

	int result = 0;
	SR_EventListener *listener = NULL;

	if (pp_listener == NULL || cb == NULL || cb->notify == NULL) {
		loge(ERRPARAM);
		return ERRPARAM;
	}

	if ((listener = (SR_EventListener *) calloc(1, sizeof(SR_EventListener))) == NULL) {
		loge(ERRMEMORY);
		return ERRMEMORY;
	}

	listener->cb = cb;
	result = sr_pipe_create(sizeof(SR_Event) * EVENT_QUEUE_SIZE, &(listener->pipe));
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
	result = pthread_create(&(listener->tid), NULL, sr___event_listener___loop, listener);
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

void sr_event_listener_release(SR_EventListener **pp_listener)
{
	logd("enter\n");

	if (pp_listener && *pp_listener) {
		SR_EventListener *listener = *pp_listener;
		*pp_listener = NULL;
		if (listener->tid != 0){
			SETFALSE(listener->running);
			while(ISFALSE(listener->stopped)){
				sr_mutex_lock(listener->mutex);
				sr_mutex_broadcast(listener->mutex);
				sr_mutex_unlock(listener->mutex);
				nanosleep((const struct timespec[]){{0, 10L}}, NULL);
			}
			pthread_join(listener->tid, NULL);
		}
		sr_pipe_release(&(listener->pipe));
		sr_mutex_release(&(listener->mutex));
		free(listener);
	}

	logd("exit\n");
}

void sr_event_listener_push_event(SR_EventListener *listener, SR_Event event)
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

void sr_event_listener_push_type(SR_EventListener *listener, int type)
{
	int result = 0;

	if (listener != NULL) {
		SR_Event event = { type, 0 };
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

void sr_event_listener_push_error(SR_EventListener *listener, int errorcode)
{
	int result = 0;

	if (listener != NULL) {
		SR_Event event = { .type = SR_EVENT_ERROR, .i32 = errorcode };
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

struct SR_SynchronousClock {

	bool fixed_audio;
	bool fixed_video;

	int64_t current;
	int64_t duration;
	int64_t audio_duration;
	int64_t first_audio_time;
	int64_t first_video_time;

	int64_t begin_time;

	SR_Mutex *mutex;
};

int sr_synchronous_clock_create(SR_SynchronousClock **pp_sync_clock)
{
	int result = 0;
	SR_SynchronousClock *sync_clock = (SR_SynchronousClock *)malloc(sizeof(SR_SynchronousClock));

	if (sync_clock == NULL){
		loge(ERRMEMORY);
		return ERRMEMORY;
	}

	if ((result = sr_mutex_create(&(sync_clock->mutex))) != 0){
		sr_synchronous_clock_release(&(sync_clock));
		loge(result);
		return result;
	}

	sr_synchronous_clock_reboot(sync_clock);

	*pp_sync_clock = sync_clock;

	return 0;
}

void sr_synchronous_clock_release(SR_SynchronousClock **pp_sync_clock)
{
	if (pp_sync_clock && *pp_sync_clock){
		SR_SynchronousClock *sync_clock = *pp_sync_clock;
		*pp_sync_clock = NULL;
		sr_mutex_release(&(sync_clock->mutex));
		free(sync_clock);
	}
}

void sr_synchronous_clock_reboot(SR_SynchronousClock *sync_clock)
{
	if (sync_clock){
		sr_mutex_lock(sync_clock->mutex);
		sync_clock->fixed_audio = false;
		sync_clock->fixed_video = false;
		sync_clock->current = 0;
		sync_clock->duration = 0;
		sync_clock->audio_duration = 0;
		sync_clock->first_audio_time = 0;
		sync_clock->first_video_time = 0;
		sync_clock->begin_time = 0;
		sr_mutex_unlock(sync_clock->mutex);
	}
}

int64_t sr_synchronous_clock_get_duration(SR_SynchronousClock *sync_clock)
{
	if (sync_clock){
		return sync_clock->duration;
	}
	return 0;
}

int64_t sr_synchronous_clock_update_audio_time(SR_SynchronousClock *sync_clock,
											   int64_t microsecond)
{
	if (sync_clock == NULL){
		loge(ERRPARAM);
		return 0;
	}

	sr_mutex_lock(sync_clock->mutex);

	if (!sync_clock->fixed_audio){
		sync_clock->fixed_audio = true;
		sync_clock->first_audio_time = microsecond;
		sync_clock->audio_duration = microsecond;
		sync_clock->current = microsecond;
		if (sync_clock->begin_time == 0){
			sync_clock->begin_time = sr_timing_start();
			sync_clock->begin_time -= sync_clock->first_audio_time;
		}else{
			if (sync_clock->first_audio_time > sync_clock->first_video_time){
				sync_clock->begin_time -= (sync_clock->first_audio_time - sync_clock->first_video_time);
			}
		}
	}else{
		sync_clock->audio_duration = microsecond;
		sync_clock->current = sr_timing_complete(sync_clock->begin_time);
	}

	sync_clock->duration = sync_clock->current;

	microsecond = sync_clock->audio_duration - sync_clock->duration;

	sr_mutex_unlock(sync_clock->mutex);

	return microsecond;
}

int64_t sr_synchronous_clock_update_video_time(SR_SynchronousClock *sync_clock,
											   int64_t microsecond)
{
	if (sync_clock == NULL){
		loge(ERRPARAM);
		return 0;
	}

	sr_mutex_lock(sync_clock->mutex);

	if (!sync_clock->fixed_video){
		sync_clock->fixed_video = true;
		sync_clock->first_video_time = microsecond;
		sync_clock->current = microsecond;

		if (sync_clock->begin_time == 0){
			sync_clock->begin_time = sr_timing_start();
			sync_clock->begin_time -= sync_clock->first_video_time;
		}else{
			if (sync_clock->first_audio_time < sync_clock->first_video_time){
				sync_clock->begin_time -= (sync_clock->first_video_time - sync_clock->first_audio_time);
			}
		}

	}else{
		sync_clock->current = sr_timing_complete(sync_clock->begin_time);
	}

	if (sync_clock->audio_duration != 0){
		microsecond -= sync_clock->audio_duration;
	}else{
		sync_clock->duration = sync_clock->current;
		microsecond -= sync_clock->duration;
	}

	sr_mutex_unlock(sync_clock->mutex);

	return microsecond;
}

///////////////////////////////////////
//self-reliance synchronous manager implement
///////////////////////////////////////

struct SR_SynchronousManager{
	bool audio_rendering;
	bool video_rendering;
	SR_SynchronousManager_Callback *callback;
	SR_MediaTransmission *transmission;
	SR_SynchronousClock *sync_clock;
	pthread_t audio_tid;
	pthread_t video_tid;
};

static void* audio_rendering_loop(void *p)
{
	int result = 0;
	int64_t delay = 0;
	int64_t begin_time = 0;
	int64_t used_time = 0;
	SR_MediaFrame *frame = NULL;
	SR_SynchronousManager *renderer = (SR_SynchronousManager *)p;

	while(ISTRUE(renderer->audio_rendering)){

		begin_time = sr_timing_start();

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

		used_time = sr_timing_complete(begin_time);

		if (delay > 0){
			nanosleep((const struct timespec[]){{0, delay * 1000L}}, NULL);
		}

		if (frame->data_type != SR_FRAME_DATA_TYPE_MEDIA_FORMAT){
			delay = sr_synchronous_clock_update_audio_time(renderer->sync_clock, frame->timestamp);
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
	SR_MediaFrame *frame = NULL;
	SR_SynchronousManager *renderer = (SR_SynchronousManager *)p;

	while(ISTRUE(renderer->video_rendering)){

		begin_time = sr_timing_start();

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

		used_time = sr_timing_complete(begin_time);

		if (delay > 0){
			nanosleep((const struct timespec[]){{0, delay * 1000L}}, NULL);
		}

		if (frame->data_type != SR_FRAME_DATA_TYPE_MEDIA_FORMAT){
			delay = sr_synchronous_clock_update_video_time(renderer->sync_clock, frame->timestamp);
			delay -= used_time;
		}

		sr_media_frame_release(&frame);
	}

	return NULL;
}

int sr_synchronous_manager_create(SR_MediaTransmission *transmission,
								  SR_SynchronousManager_Callback *callback,
								  SR_SynchronousManager **pp_manager)
{
	int result = 0;
	SR_SynchronousManager *manager = NULL;

	if (transmission == NULL
			|| callback == NULL
			|| callback->render_audio == NULL
			|| callback->render_video == NULL
			|| transmission->receive_frame == NULL
			|| pp_manager == NULL){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	manager = (SR_SynchronousManager *)malloc(sizeof(SR_SynchronousManager));
	if (manager == NULL){
		loge(ERRMEMORY);
		return ERRMEMORY;
	}
	memset(manager, 0, sizeof(SR_SynchronousManager));

	manager->callback = callback;
	manager->transmission = transmission;

	result = sr_synchronous_clock_create(&(manager->sync_clock));
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

void sr_synchronous_manager_release(SR_SynchronousManager **pp_manager)
{
	if (pp_manager && *pp_manager){
		SR_SynchronousManager *manager = *pp_manager;
		*pp_manager = NULL;
		if (manager->audio_tid != 0){
			SETFALSE(manager->audio_rendering);
			pthread_join(manager->audio_tid, NULL);
		}
		if (manager->video_tid != 0){
			SETFALSE(manager->video_rendering);
			pthread_join(manager->video_tid, NULL);
		}
		sr_synchronous_clock_release(&(manager->sync_clock));
		free(manager);
	}
}
