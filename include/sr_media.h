/*
 * sr_media.h
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
	SR_FRAME_DATA_TYPE_MEDIA_FORMAT
};

enum {
	SR_FRAME_VIDEO_FLAG_NONE = 0x00,
	SR_FRAME_VIDEO_FLAG_KEY_FRAME = 0x02
};

enum {
	SR_FRAME_AUDIO_FLAG_NONE = 0x00,
};

typedef struct SR_MediaFrame
{
	int media_type;
	int data_type;
	int frame_flag;

	int64_t timestamp;

	uint32_t data_size;
	uint8_t *frame_data;

	uint32_t frame_size;
	uint8_t *frame_header;

	SR_QUEUE_ENABLE(SR_MediaFrame);

}SR_MediaFrame;

SR_QUEUE_DEFINE(SR_MediaFrame);


extern int sr_media_frame_create(SR_MediaFrame **pp_frame);
extern void sr_media_frame_release(SR_MediaFrame **pp_frame);
extern int sr_media_frame_fill_header(SR_MediaFrame *frame);
extern int sr_media_frame_parse_header(SR_MediaFrame *frame);
extern int sr_media_frame_fill_data(SR_MediaFrame *frame, uint8_t *data, uint32_t size);


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

typedef struct SR_AudioFormat{
	int32_t codec_type;
	int32_t bit_rate;
	int32_t channels;
	int32_t sample_rate;
	int32_t sample_type;
	int32_t bit_per_sample;
	int32_t byte_per_sample;
	int32_t sample_per_frame;
	uint8_t extend_data_size;
	uint8_t extend_data[255];
}SR_AudioFormat;

typedef struct SR_VideoFormat{
	int32_t codec_type;
	int32_t bit_rate;
	int32_t width;
	int32_t height;
	int32_t pixel_type;
	int32_t byte_per_pixel;
	int32_t frame_per_second;
	int32_t key_frame_interval;
	uint8_t extend_data_size;
	uint8_t extend_data[255];
}SR_VideoFormat;

extern SR_MediaFrame* sr_audio_format_to_media_frame(SR_AudioFormat *format);
extern SR_MediaFrame* sr_video_format_to_media_frame(SR_VideoFormat *format);

extern SR_AudioFormat* sr_media_frame_to_audio_format(SR_MediaFrame *frame);
extern SR_VideoFormat* sr_media_frame_to_video_format(SR_MediaFrame *frame);

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
	SR_EVENT_USER_DEFINE
};

typedef struct SR_Event{
	int type;
	union
	{
		int32_t i32;
		uint32_t u32;
		int64_t i64;
		uint64_t u64;
		double f100;
	};
}SR_Event;

typedef struct SR_Event_Callback{
	void *handler;
	void (*notify)(struct SR_Event_Callback *cb, SR_Event event);
} SR_Event_Callback;

typedef struct SR_EventListener SR_EventListener;

extern int sr_event_listener_create(SR_Event_Callback *cb, SR_EventListener **pp_listener);
extern void sr_event_listener_release(SR_EventListener **pp_listener);

extern void sr_event_listener_push_type(SR_EventListener *listener, int type);
extern void sr_event_listener_push_error(SR_EventListener *listener, int errorcode);
extern void sr_event_listener_push_event(SR_EventListener *listener, SR_Event event);

///////////////////////////////////////
//self-reliance media synchronous clock define
///////////////////////////////////////

typedef struct SR_SynchronousClock SR_SynchronousClock;

extern int sr_synchronous_clock_create(SR_SynchronousClock **pp_clock);
extern void sr_synchronous_clock_release(SR_SynchronousClock **pp_clock);
extern void sr_synchronous_clock_reboot(SR_SynchronousClock *sync_clock);
extern int64_t sr_synchronous_clock_get_duration(SR_SynchronousClock *sync_clock);
extern int64_t sr_synchronous_clock_update_audio_time(SR_SynchronousClock *sync_clock, int64_t microsecond);
extern int64_t sr_synchronous_clock_update_video_time(SR_SynchronousClock *sync_clock, int64_t microsecond);

///////////////////////////////////////
//self-reliance media transmission define
///////////////////////////////////////

typedef struct SR_MediaTransmission{
	int (*send_frame)(struct SR_MediaTransmission *transmission, SR_MediaFrame *frame);
	int (*receive_frame)(struct SR_MediaTransmission *transmission, SR_MediaFrame **pp_frame, int frame_type);
	void (*flush)(struct SR_MediaTransmission *transmission);
	void (*stop)(struct SR_MediaTransmission *transmission);
	struct sr_media_transmission_protocol_t *protocol;
}SR_MediaTransmission;

///////////////////////////////////////
//self-reliance media synchronous manager define
///////////////////////////////////////

typedef struct SR_SynchronousManager_Callback{
	void *renderer;
	void (*render_audio)(struct SR_SynchronousManager_Callback *cb, SR_MediaFrame *frame);
	void (*render_video)(struct SR_SynchronousManager_Callback *cb, SR_MediaFrame *frame);
}SR_SynchronousManager_Callback;

typedef struct SR_SynchronousManager SR_SynchronousManager;

extern int sr_synchronous_manager_create(SR_MediaTransmission *transmission,
										 SR_SynchronousManager_Callback *callback,
										 SR_SynchronousManager **pp_manager);
extern void sr_synchronous_manager_release(SR_SynchronousManager **pp_manager);

///////////////////////////////////////
//self-reliance media encoder define
///////////////////////////////////////

typedef struct SR_AudioEncoder_Base{
	int (*init)(struct SR_AudioEncoder_Base *, SR_AudioFormat *);
	int (*encode)(struct SR_AudioEncoder_Base *, SR_MediaFrame *);
	void (*flush)(struct SR_AudioEncoder_Base *);
	void (*release)(struct SR_AudioEncoder_Base *);
	struct sr_audio_encoder_implement_t *implement;
}SR_AudioEncoder_Base;

typedef struct SR_VideoEncoder_Base{
	int (*init)(struct SR_VideoEncoder_Base *, SR_VideoFormat *);
	int (*encode)(struct SR_VideoEncoder_Base *, SR_MediaFrame *);
	void (*flush)(struct SR_VideoEncoder_Base *);
	void (*release)(struct SR_VideoEncoder_Base *);
	struct sr_video_encoder_implement_t *implement;
}SR_VideoEncoder_Base;

///////////////////////////////////////
//self-reliance media encoder define
///////////////////////////////////////

typedef struct SR_AudioDecoder_Base{
	int (*init)(struct SR_AudioDecoder_Base *, SR_AudioFormat *);
	int (*encode)(struct SR_AudioDecoder_Base *, SR_MediaFrame *);
	void (*flush)(struct SR_AudioDecoder_Base *);
	void (*release)(struct SR_AudioDecoder_Base *);
	struct sr_audio_decoder_implement_t *implement;
}SR_AudioDecoder_Base;

typedef struct SR_VideoDecoder_Base{
	int (*init)(struct SR_VideoDecoder_Base *, SR_VideoFormat *);
	int (*encode)(struct SR_VideoDecoder_Base *, SR_MediaFrame *);
	void (*flush)(struct SR_VideoDecoder_Base *);
	void (*release)(struct SR_VideoDecoder_Base *);
	struct sr_video_decoder_implement_t *implement;
}SR_VideoDecoder_Base;


#endif /* INCLUDE_SR_MEDIA_H_ */
