/*
 * sr_message.h
 *
 *  Created on: 2017年6月18日
 *      Author: kly
 */

#ifndef INCLUDE_SR_MESSAGE_H_
#define INCLUDE_SR_MESSAGE_H_

#include "stdint.h"

typedef struct Sr_message{
	int event;
	union
	{
		double f64;
		int32_t i32;
		int64_t i64;
		uint32_t u32;
		uint64_t u64;
	};
	int size;
	void *data;
}Sr_message;

typedef struct Sr_message_callback{
	void *handler;
	void (*notify)(struct Sr_message_callback *cb, Sr_message msg);
}Sr_message_callback;

typedef struct Sr_message_listener Sr_message_listener;

extern int sr_message_listener_create(Sr_message_callback *cb, Sr_message_listener **pp_listener);
extern void sr_message_listener_release(Sr_message_listener **pp_listener);

extern int sr_message_listener_pop(Sr_message_listener *listener, Sr_message *msg);
extern int sr_message_listener_push(Sr_message_listener *listener, Sr_message msg);
extern int sr_message_listener_push_event(Sr_message_listener *listener, int event);

#endif /* INCLUDE_SR_MESSAGE_H_ */
