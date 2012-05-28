/*
 * Sample Rate Converter Plugin for Audacious
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

#include <stdio.h>

#include <samplerate.h>

#define RESAMPLE_ERROR(e) fprintf (stderr, "resample: %s\n", src_strerror (e))

extern const int common_rates[];
extern const int n_common_rates;

extern int converted_rates[];
extern int fallback_rate;
extern int method;

void resample_config_load (void);
void resample_config_save (void);

int resample_init (void);
void resample_cleanup (void);
void resample_start (int * channels, int * rate);
void resample_process (float * * data, int * samples);
void resample_flush (void);
void resample_finish (float * * data, int * samples);
