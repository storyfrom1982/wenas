/*
 * sr_media.h
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

#ifndef INCLUDE_SR_MEDIA_H_
#define INCLUDE_SR_MEDIA_H_


#include "sr_queue.h"


///////////////////////////////////////
//self-reliance media frame define
///////////////////////////////////////

enum {
	SR_FRAME_MEDIA_TYPE_VIDEO = 0,
	SR_FRAME_MEDIA_TYPE_AUDIO
};

enum {
	SR_FRAME_DATA_TYPE_MEDIA_DATA = 0,
	SR_FRAME_DATA_TYPE_MEDIA_CONFIG
};

enum {
	SR_FRAME_VIDEO_FLAG_NONE = 0x00,
	SR_FRAME_VIDEO_FLAG_KEY_FRAME = 0x02
};

enum {
	SR_FRAME_AUDIO_FLAG_NONE = 0x00,
};

typedef struct Sr_media_frame
{
	int media_type;
	int data_type;
	int frame_flag;

	int64_t timestamp;

	uint32_t data_size;
	uint8_t *frame_data;

	uint32_t frame_size;
	uint8_t *frame_header;

	SR_QUEUE_ENABLE(Sr_media_frame);

}Sr_media_frame;

SR_QUEUE_DEFINE(Sr_media_frame);


extern int sr_media_frame_create(Sr_media_frame **pp_frame);
extern void sr_media_frame_release(Sr_media_frame **pp_frame);
extern int sr_media_frame_fill_header(Sr_media_frame *frame);
extern int sr_media_frame_parse_header(Sr_media_frame *frame);
extern int sr_media_frame_fill_data(Sr_media_frame *frame, uint8_t *data, uint32_t size);


///////////////////////////////////////
//self-reliance media format define
///////////////////////////////////////

enum {
	SR_MEDIA_CODEC_EXTEND = 10000,
	SR_MEDIA_CODEC_EXTEND_AAC,
	SR_MEDIA_CODEC_EXTEND_AVC
};

enum {
	SR_AUDIO_SAMPLE_U8 = 0,
	SR_AUDIO_SAMPLE_S16
};

enum {
	SR_VIDEO_PIXEL_YUV = 0,
	SR_VIDEO_PIXEL_RGB
};

typedef struct Sr_audio_config{
	int32_t codec;
	int32_t bit_rate;
	int32_t channels;
	int32_t sample_rate;
	int32_t sample_type;
	int32_t bit_per_sample;
	int32_t byte_per_sample;
	int32_t sample_per_frame;
	uint8_t extend_data_size;
	uint8_t extend_data[255];
}Sr_audio_config;

typedef struct Sr_video_config{
	int32_t codec;
	int32_t bit_rate;
	int32_t width;
	int32_t height;
	int32_t pixel_type;
	int32_t byte_per_pixel;
	int32_t frame_per_second;
	int32_t key_frame_interval;
	uint8_t extend_data_size;
	uint8_t extend_data[255];
}Sr_video_config;

extern Sr_media_frame* sr_audio_config_to_media_frame(Sr_audio_config *config);
extern Sr_media_frame* sr_video_config_to_media_frame(Sr_video_config *config);

extern Sr_audio_config* sr_media_frame_to_audio_config(Sr_media_frame *frame);
extern Sr_video_config* sr_media_frame_to_video_config(Sr_media_frame *frame);

///////////////////////////////////////
//self-reliance media event define
///////////////////////////////////////

enum{

	SR_EVENT_ERROR = -1,
	SR_EVENT_INITIALIZE,
	SR_EVENT_RUNNING,
	SR_EVENT_PAUSING,
	SR_EVENT_SEEKING,
	SR_EVENT_BUFFERING,
	SR_EVENT_TERMINATION,
	SR_EVENT_DURATION,
	SR_EVENT_USER_DEFINE
};

typedef struct Sr_event{
	int type;
	union
	{
		int32_t i32;
		uint32_t u32;
		int64_t i64;
		uint64_t u64;
		double f100;
	};
}Sr_event;

typedef struct Sr_event_callback{
	void *handler;
	void (*notify)(struct Sr_event_callback *cb, Sr_event event);
} Sr_event_callback;

typedef struct Sr_event_listener Sr_event_listener;

extern int sr_event_listener_create(Sr_event_callback *cb, Sr_event_listener **pp_listener);
extern void sr_event_listener_release(Sr_event_listener **pp_listener);

extern void sr_event_listener_push_type(Sr_event_listener *listener, int type);
extern void sr_event_listener_push_error(Sr_event_listener *listener, int errorcode);
extern void sr_event_listener_push_event(Sr_event_listener *listener, Sr_event event);

///////////////////////////////////////
//self-reliance media synchronous clock define
///////////////////////////////////////

typedef struct Sr_media_clock Sr_media_clock;

extern int sr_media_clock_create(Sr_media_clock **pp_clock);
extern void sr_media_clock_release(Sr_media_clock **pp_clock);
extern void sr_media_clock_reboot(Sr_media_clock *sync_clock);
extern int64_t sr_media_clock_get_duration(Sr_media_clock *sync_clock);
extern int64_t sr_media_clock_update_audio_time(Sr_media_clock *sync_clock, int64_t microsecond);
extern int64_t sr_media_clock_update_video_time(Sr_media_clock *sync_clock, int64_t microsecond);

///////////////////////////////////////
//self-reliance media transmission define
///////////////////////////////////////

typedef struct Sr_media_transmission{
	int (*send_frame)(struct Sr_media_transmission *transmission, Sr_media_frame *frame);
	int (*receive_frame)(struct Sr_media_transmission *transmission, Sr_media_frame **pp_frame, int frame_type);
	void (*flush)(struct Sr_media_transmission *transmission);
	void (*stop)(struct Sr_media_transmission *transmission);
	struct sr_media_transmission_protocol_t *protocol;
}Sr_media_transmission;

///////////////////////////////////////
//self-reliance media synchronous manager define
///////////////////////////////////////

typedef struct Sr_synchronous_manager_callback{
	void *renderer;
	void (*render_audio)(struct Sr_synchronous_manager_callback *cb, Sr_media_frame *frame);
	void (*render_video)(struct Sr_synchronous_manager_callback *cb, Sr_media_frame *frame);
}Sr_synchronous_manager_callback;

typedef struct Sr_synchronous_manager Sr_synchronous_manager;

extern int sr_synchronous_manager_create(Sr_media_transmission *transmission,
										 Sr_synchronous_manager_callback *callback,
										 Sr_synchronous_manager **pp_manager);
extern void sr_synchronous_manager_release(Sr_synchronous_manager **pp_manager);

///////////////////////////////////////
//self-reliance media encoder define
///////////////////////////////////////

typedef struct Sr_audio_encoder_base{
	int (*init)(struct Sr_audio_encoder_base *, Sr_audio_config *);
	int (*encode)(struct Sr_audio_encoder_base *, Sr_media_frame *);
	void (*flush)(struct Sr_audio_encoder_base *);
	void (*release)(struct Sr_audio_encoder_base *);
	struct Sr_audio_encoder_implement_t *implement;
}Sr_audio_encoder_base;

typedef struct Sr_video_encoder_base{
	int (*init)(struct Sr_video_encoder_base *, Sr_video_config *);
	int (*encode)(struct Sr_video_encoder_base *, Sr_media_frame *);
	void (*flush)(struct Sr_video_encoder_base *);
	void (*release)(struct Sr_video_encoder_base *);
	struct Sr_video_encoder_implement_t *implement;
}Sr_video_encoder_base;

///////////////////////////////////////
//self-reliance media encoder define
///////////////////////////////////////

typedef struct Sr_audio_decoder_base{
	int (*init)(struct Sr_audio_decoder_base *, Sr_audio_config *);
	int (*encode)(struct Sr_audio_decoder_base *, Sr_media_frame *);
	void (*flush)(struct Sr_audio_decoder_base *);
	void (*release)(struct Sr_audio_decoder_base *);
	struct Sr_audio_decoder_implement_t *implement;
}Sr_audio_decoder_base;

typedef struct Sr_video_decoder_base{
	int (*init)(struct Sr_video_decoder_base *, Sr_video_config *);
	int (*encode)(struct Sr_video_decoder_base *, Sr_media_frame *);
	void (*flush)(struct Sr_video_decoder_base *);
	void (*release)(struct Sr_video_decoder_base *);
	struct Sr_video_decoder_implement_t *implement;
}Sr_video_decoder_base;


#endif /* INCLUDE_SR_MEDIA_H_ */
