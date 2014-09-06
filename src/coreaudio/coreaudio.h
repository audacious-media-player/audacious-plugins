/*
 * CoreAudio Output Plugin for Audacious
 * Copyright 2010 John Lindgren
 * Copyright 2014 William Pitcock
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#ifndef AUDACIOUS_COREAUDIO_H
#define AUDACIOUS_COREAUDIO_H

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

/* coreaudio.cc */
namespace coreaudio {

bool init (void);
void cleanup (void);
void get_volume (int * left, int * right);
void set_volume (int left, int right);
bool open_audio (int format, int rate, int chan);
void close_audio (void);
int buffer_free (void);
void period_wait (void);
void write_audio (void * data, int len);
void drain (void);
int output_time (void);
void pause (bool pause);
void flush (int time);

};

#endif
