/*
 * sr_media_file.h
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

#ifndef INCLUDE_SR_MEDIA_FILE_H_
#define INCLUDE_SR_MEDIA_FILE_H_


#include "sr_media.h"


extern int sr_file_protocol_create_writer(const char *path,
		SR_EventListener *listener,
		SR_MediaTransmission **pp_writer);

extern int sr_file_protocol_create_reader(const char *path,
		SR_EventListener *listener,
		SR_MediaTransmission **pp_reader);

extern void sr_file_protocol_release(SR_MediaTransmission **pp_transmission);

extern void sr_file_protocol_stop(SR_MediaTransmission *transmission);


#endif /* INCLUDE_SR_MEDIA_FILE_H_ */
