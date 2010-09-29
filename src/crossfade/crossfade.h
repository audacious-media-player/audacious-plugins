/*
 * Crossfade Plugin for Audacious
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

extern int crossfade_length;

void crossfade_config_load (void);
void crossfade_config_save (void);
void crossfade_show_channels_message (void);
void crossfade_show_rate_message (void);

int crossfade_init (void);
void crossfade_cleanup (void);
void crossfade_start (int * channels, int * rate);
void crossfade_process (float * * data, int * samples);
void crossfade_flush (void);
void crossfade_finish (float * * data, int * samples);
int crossfade_decoder_to_output_time (int time);
int crossfade_output_to_decoder_time (int time);
