/*
 * media.c
 *
 *  Created on: 2017年3月1日
 *      Author: kly
 */



#include "sr_media.h"
#include "sr_media_file.h"

#include "sr_log.h"
#include "sr_atom.h"
#include "sr_byte.h"
#include "sr_error.h"
#include "sr_time.h"
#include "sr_pipe.h"
#include "sr_mutex.h"
#include "sr_memory.h"
#include "sr_queue.h"

static SR_MediaFrame* make_video_head()
{
	SR_VideoFormat vf_in, *vf_out;
	SR_MediaFrame *frame = NULL;

	vf_in.codec_type = SR_MEDIA_CODEC_EXTEND_AVC;
	vf_in.bit_rate = 1024 * 1024 * 1024;
	vf_in.pixel_type = SR_VIDEO_PIXEL_YUV;
	vf_in.width = 960;
	vf_in.height = 540;
	vf_in.byte_per_pixel = 3;
	vf_in.frame_per_second = 25;
	vf_in.key_frame_interval = 25;

	vf_in.extend_data_size = 10;
	memset(vf_in.extend_data, 'v', 10);

	frame = sr_video_format_to_media_frame(&vf_in);

	return frame;
}


static SR_MediaFrame* make_audio_head()
{
	SR_AudioFormat af_in, *af_out;
	SR_MediaFrame *frame = NULL;

	af_in.codec_type = SR_MEDIA_CODEC_EXTEND_AAC;
	af_in.sample_type = SR_AUDIO_SAMPLE_S16;
	af_in.bit_rate = 1024 * 1024 * 128;
	af_in.channels = 2;
	af_in.sample_rate = 44100;
	af_in.bit_per_sample = 16;
	af_in.byte_per_sample = 2;
	af_in.sample_per_frame = 1024;

	af_in.extend_data_size = 10;
	memset(af_in.extend_data, 'a', 10);

	frame = sr_audio_format_to_media_frame(&af_in);

	return frame;
}



static SR_MediaFrame* make_frame(int media_type, int64_t timestamp)
{
	SR_MediaFrame *frame = NULL;

	sr_media_frame_create(&frame);
	frame->media_type = media_type;
	frame->data_type = SR_FRAME_DATA_TYPE_MEDIA_DATA;
	frame->frame_flag = SR_FRAME_VIDEO_FLAG_KEY_FRAME;
	frame->timestamp = timestamp;

	uint8_t data[1024] = {0};
	if (media_type == SR_FRAME_MEDIA_TYPE_AUDIO){
		data[0] = 'a';
		memset(data + 1, 'v', 1024 - 2);
		sr_media_frame_fill_data(frame, data, 1024);
	}else if (media_type == SR_FRAME_MEDIA_TYPE_VIDEO){
		data[0] = 'v';
		memset(data + 1, 'a', 1024 - 2);
		sr_media_frame_fill_data(frame, data, 1024);
	}

	return frame;
}


static bool running = false;

static void receive_writer_event(SR_Event_Callback *cb, SR_Event event)
{
	logd("receive writer event %d\n", event.type);
}

static void receive_reader_event(SR_Event_Callback *cb, SR_Event event)
{
	if (event.type == SR_EVENT_RUNNING){
		SETTRUE(running);
	}else if (event.type == SR_EVENT_TERMINATION){

	}
	logd("receive reader event %d\n", event.type);
}

static SR_Event_Callback mec = {0};


static int frame_count = 0;

static void renderer_audio(SR_SynchronousManager_Callback *callback, SR_MediaFrame *frame)
{
	int count = frame_count;
	if (frame->data_type == SR_FRAME_DATA_TYPE_MEDIA_DATA){
		logd("================================================%d\n"
				"frame frame type %d\n"
				"frame data type %d\n"
				"frame frame flag %d\n"
				"frame timestamp %llu\n"
				"frame data size %u\n"
				"frame data buffer %s\n"
				"\n\n",
				count,
				frame->media_type,
				frame->data_type,
				frame->frame_flag,
				frame->timestamp,
				frame->data_size,
				frame->frame_data);
	}else{
		SR_AudioFormat *af_out = sr_media_frame_to_audio_format(frame);
		logd("================================================%d\n"
				"frame frame type %d\n"
				"frame data type %d\n"
				"frame frame flag %d\n"
				"####################\n"
				"codec type %d\n"
				"bit_rate %d\n"
				"channels %d\n"
				"sample rata %d\n"
				"sample type %d\n"
				"bit per sample %d\n"
				"byte per sample %d\n"
				"sample per frame %d\n"
				"extend data size %d\n"
				"extend data %s\n"
				"\n\n",
				count,
				frame->media_type,
				frame->data_type,
				frame->frame_flag,
				af_out->codec_type,
				af_out->bit_rate,
				af_out->channels,
				af_out->sample_rate,
				af_out->sample_type,
				af_out->bit_per_sample,
				af_out->byte_per_sample,
				af_out->sample_per_frame,
				af_out->extend_data_size,
				af_out->extend_data);
		free(af_out);
	}

	SR_ATOM_ADD(frame_count, 1);
}

static void renderer_video(SR_SynchronousManager_Callback *callback, SR_MediaFrame *frame)
{
	int count = frame_count;
	if (frame->data_type == SR_FRAME_DATA_TYPE_MEDIA_DATA){
		logd("================================================%d\n"
				"frame frame type %d\n"
				"frame data type %d\n"
				"frame frame flag %d\n"
				"frame timestamp %llu\n"
				"frame data size %u\n"
				"frame data buffer %s\n"
				"\n\n",
				count,
				frame->media_type,
				frame->data_type,
				frame->frame_flag,
				frame->timestamp,
				frame->data_size,
				frame->frame_data);
	}else{
		SR_VideoFormat *vf_out = sr_media_frame_to_video_format(frame);
		logd("================================================%d\n"
				"frame frame type %d\n"
				"frame data type %d\n"
				"frame frame flag %d\n"
				"####################\n"
				"codec type %d\n"
				"bit_rate %d\n"
				"width %d\n"
				"height %d\n"
				"pixel_type %d\n"
				"byte_per_pixel %d\n"
				"frame_per_second %d\n"
				"key_frame_interval %d\n"
				"extend data size %d\n"
				"extend data %s\n"
				"\n\n",
				count,
				frame->media_type,
				frame->data_type,
				frame->frame_flag,
				vf_out->codec_type,
				vf_out->bit_rate,
				vf_out->width,
				vf_out->height,
				vf_out->pixel_type,
				vf_out->byte_per_pixel,
				vf_out->frame_per_second,
				vf_out->key_frame_interval,
				vf_out->extend_data_size,
				vf_out->extend_data);
		free(vf_out);
	}

	SR_ATOM_ADD(frame_count, 1);
}

static SR_SynchronousManager_Callback manager_callback = {0};


int main(int argc, char *argv[])
{
	int result = 0;

	SR_MediaFrame *frame = NULL;

	SR_MediaTransmission *tm = NULL;
	SR_EventListener *el = NULL;
	SR_SynchronousManager *manager = NULL;

	result = sr_memory_default_init();
	if (result != 0){
		exit(0);
	}


	//////////////////////////////////////////////////////////////
	//write
	//////////////////////////////////////////////////////////////


	mec.notify = receive_writer_event;
	mec.handler = NULL;
	result = sr_event_listener_create(&mec, &el);
	if (result != 0){
		exit(0);
	}

	logd("step ================== 1\n");

	result = sr_file_protocol_create_writer("/tmp/test.av", el, &tm);
	if (result != 0){
		exit(0);
	}

	frame = make_audio_head();
	while ((result = tm->send_frame(tm, frame)) != 0){
		if (result == ERRTRYAGAIN){
			nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
			continue;
		}
		break;
	}

	if (result != 0){
		exit(0);
	}


	frame = make_video_head();
	while ((result = tm->send_frame(tm, frame)) != 0){
		if (result == ERRTRYAGAIN){
			nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
			continue;
		}
		break;
	}

	if (result != 0){
		exit(0);
	}


	logd("step ================== 2\n");

	for (int i = 0; i < 10; ++i){
		logd("write frame %d\n", i);
		if (i%2){
			frame = make_frame(SR_FRAME_MEDIA_TYPE_VIDEO, i * 1000000);
		}else{
			frame = make_frame(SR_FRAME_MEDIA_TYPE_AUDIO, i * 1000000);
		}
		while ((result = tm->send_frame(tm, frame)) != 0){
			if (result == ERRTRYAGAIN){
				nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
				continue;
			}
			break;
		}
		if (result != 0){
			loge(result);
			break;
		}
	}

	logd("step ================== 3\n");

	tm->flush(tm);
	logd("step ================== 3.1\n");
	sr_file_protocol_release(&tm);
	logd("step ================== 3.2\n");
	sr_event_listener_release(&el);



	//////////////////////////////////////////////////////////////
	//read
	//////////////////////////////////////////////////////////////


	logd("step ================== 4\n");

	mec.notify = receive_reader_event;
	mec.handler = NULL;
	result = sr_event_listener_create(&mec, &el);
	if (result != 0){
		exit(0);
	}

	logd("step ================== 5\n");

	result = sr_file_protocol_create_reader("/tmp/test.av", el, &tm);
	if (result != 0){
		exit(0);
	}

	manager_callback.render_audio = renderer_audio;
	manager_callback.render_video = renderer_video;
	result = sr_synchronous_manager_create(tm, &manager_callback, &manager);
	if (result != 0){
		exit(0);
	}

	logd("step ================== 6\n");

	while(ISFALSE(running)){
		usleep(1000000);
	}

	tm->flush(tm);
	sr_file_protocol_release(&tm);
	sr_synchronous_manager_release(&manager);
	sr_event_listener_release(&el);


	logd("frame count ================== %d\n", frame_count);

	sr_memory_debug(sr_log_info);

	sr_memory_release();

	return 0;
}

