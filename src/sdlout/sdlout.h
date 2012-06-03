/*
 * SDL Output Plugin for Audacious
 * Copyright 2010 John Lindgren
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

#ifndef AUDACIOUS_SDLOUT_H
#define AUDACIOUS_SDLOUT_H

/* sdlout.c */
int sdlout_init (void);
void sdlout_cleanup (void);
void sdlout_get_volume (int * left, int * right);
void sdlout_set_volume (int left, int right);
int sdlout_open_audio (int format, int rate, int chan);
void sdlout_close_audio (void);
int sdlout_buffer_free (void);
void sdlout_period_wait (void);
void sdlout_write_audio (void * data, int len);
void sdlout_drain (void);
int sdlout_written_time (void);
int sdlout_output_time (void);
void sdlout_pause (int pause);
void sdlout_flush (int time);
void sdlout_set_written_time (int time);

#endif
