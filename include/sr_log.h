/*
 * sr_log.h
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


#ifndef INCLUDE_SR_LOG_H_
#define INCLUDE_SR_LOG_H_


#define	SR_LOG_DEBUG			0
#define	SR_LOG_INFO				1
#define	SR_LOG_WARNING			2
#define	SR_LOG_ERROR			3


extern void sr_log_setup(void (*log_callback) (int level, const char *tag, const char *log));
extern void sr_log_warning(const char *file, const char *function, int line, const char *format, ...);
extern void sr_log_error(const char *file, const char *function, int line, int errorcode);
extern void sr_log_debug(const char *file, const char *function, int line, const char *format, ...);
extern void sr_log_info(const char *format, ...);


#ifdef logd
 #undef logd
#endif

#ifdef ___LOG_DEBUG___
#define logd(__FORMAT__, ...) \
	sr_log_debug(__FILE__, __FUNCTION__, __LINE__, __FORMAT__, ##__VA_ARGS__)
#else
#define logd(__FORMAT__, ...)	do {} while(0)
#endif


#ifdef loge
 #undef loge
#endif

#define loge(__ERROR_CODE__) \
	sr_log_error(__FILE__, __FUNCTION__, __LINE__, __ERROR_CODE__)


#ifdef logw
 #undef logw
#endif

#define logw(__FORMAT__, ...) \
	sr_log_warning(__FILE__, __FUNCTION__, __LINE__, __FORMAT__, ##__VA_ARGS__)


#ifdef logi
 #undef logi
#endif

#define logi(__FORMAT__, ...)	sr_log_info(__FORMAT__, ##__VA_ARGS__)


#endif /* INCLUDE_SR_LOG_H_ */
