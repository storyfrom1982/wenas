/*
 * sr_log.c
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


#include "sr_log.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "sr_error.h"


#define LOG_TIME_SIZE			32
#define LOG_LINE_SIZE			4096


#define PATHCLEAR(FILEPATH) \
	( strrchr( FILEPATH, '/' ) ? strrchr( FILEPATH, '/' ) + 1 : FILEPATH )


typedef struct Sr_log_callback{
	void (*log_callback) (int level, const char *tag, const char *log);
}Sr_log_callback;


static void log_debug(int level, const char *tag, const char *log)
{
	if (level == SR_LOG_INFO){
		fprintf(stderr, "[%s] %s", tag, log);
	}else if (level == SR_LOG_DEBUG){
		fprintf(stderr, "[D] [%-10s] %s", tag, log);
	}else if (level == SR_LOG_WARNING){
		fprintf(stderr, "[W] [%-10s] %s", tag, log);
	}else if (level == SR_LOG_ERROR){
		fprintf(stderr, "[E] [%-10s] %s", tag, log);
	}
}


static Sr_log_callback g_logger = {log_debug};


static char* time_to_string(char *buffer, size_t length)
{
	time_t current_time = {0};
	struct tm *format_time = NULL;
	time(&current_time);
	format_time = localtime(&current_time);
	strftime(buffer, length, "%F:%H:%M:%S", format_time);
	return buffer;
}


void sr_log_setup(void (*log_callback) (int level, const char *tag, const char *log))
{
	if (log_callback){
		g_logger.log_callback = log_callback;
	}
}


void sr_log_info(const char *format, ...)
{
	char log_line[LOG_LINE_SIZE] = {0};
	va_list args;
	va_start (args, format);
	vsnprintf(log_line, LOG_LINE_SIZE, format, args);
	va_end (args);
	g_logger.log_callback(SR_LOG_INFO, "I", log_line);
}


void sr_log_debug(const char *file, const char *function, int line, const char *format, ...)
{
	char log_line[LOG_LINE_SIZE] = {0};
	int i = snprintf(log_line, LOG_LINE_SIZE, "%s[%-4d] ", function, line);
	va_list args;
	va_start (args, format);
	vsnprintf(log_line + i, LOG_LINE_SIZE - i, format, args);
	va_end (args);
	g_logger.log_callback(SR_LOG_DEBUG, PATHCLEAR(file), log_line);
}


void sr_log_error(const char *file, const char *function, int line, int errorcode)
{
	char log_line[LOG_LINE_SIZE] = {0};
	char log_time[LOG_TIME_SIZE] = {0};
	snprintf(log_line, LOG_LINE_SIZE, "%s %s(%d) ERROR(%d) %s\n",
			time_to_string(log_time, LOG_TIME_SIZE),
			function, line, errorcode, strerror(errno));
	if (errorcode == ERREOF){
		g_logger.log_callback(SR_LOG_DEBUG, PATHCLEAR(file), log_line);
	}else{
		g_logger.log_callback(SR_LOG_ERROR, PATHCLEAR(file), log_line);
	}
}


void sr_log_warning(const char *file, const char *function, int line, const char *format, ...)
{
	char log_line[LOG_LINE_SIZE] = {0};
	char log_time[LOG_TIME_SIZE] = {0};
	int i = snprintf(log_line, LOG_LINE_SIZE, "%s %s[%-4d] ",
			time_to_string(log_time, LOG_TIME_SIZE), function, line);
	va_list args;
	va_start (args, format);
	vsnprintf(log_line + i, LOG_LINE_SIZE - i, format, args);
	va_end (args);
	g_logger.log_callback(SR_LOG_WARNING, PATHCLEAR(file), log_line);
}
