/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009-2012 John Lindgren
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

#ifndef AUDACIOUS_ALSA_H
#define AUDACIOUS_ALSA_H

#include <stdio.h>
#include <audacious/misc.h>
#include <libaudcore/audstrings.h>

#define ERROR(...) fprintf (stderr, "alsa: " __VA_ARGS__)

#define ERROR_NOISY(...) do { \
    SPRINTF (ERROR_NOISY_buf, "ALSA error: " __VA_ARGS__); \
    aud_interface_show_error (ERROR_NOISY_buf); \
} while (0)

#define CHECK_VAL(value, function, ...) \
do { \
    (value) = function (__VA_ARGS__); \
    if ((value) < 0) { \
        ERROR ("%s failed: %s.\n", #function, snd_strerror (value)); \
        goto FAILED; \
    } \
} while (0)

#define CHECK(function, ...) \
do { \
    int CHECK_error; \
    CHECK_VAL (CHECK_error, function, __VA_ARGS__); \
} while (0)

#define CHECK_NOISY(function, ...) \
do { \
    int CHECK_NOISY_error = function (__VA_ARGS__); \
    if (CHECK_NOISY_error < 0) { \
        ERROR_NOISY ("%s failed: %s.\n", #function, snd_strerror (CHECK_NOISY_error)); \
        goto FAILED; \
    } \
} while (0)

/* alsa.c */
int alsa_init (void);
void alsa_cleanup (void);
int alsa_open_audio (int aud_format, int rate, int channels);
void alsa_close_audio (void);
int alsa_buffer_free (void);
void alsa_write_audio (void * data, int length);
void alsa_period_wait (void);
void alsa_drain (void);
int alsa_output_time (void);
void alsa_flush (int time);
void alsa_pause (int pause);
void alsa_open_mixer (void);
void alsa_close_mixer (void);
void alsa_get_volume (int * left, int * right);
void alsa_set_volume (int left, int right);

/* config.c */
extern char * alsa_config_pcm, * alsa_config_mixer, * alsa_config_mixer_element;
extern unsigned alsa_config_mixer_element_index;
extern int alsa_config_drop_workaround, alsa_config_drain_workaround,
 alsa_config_delay_workaround;

void alsa_config_load (void);
void alsa_config_save (void);
void * alsa_create_config_widget (void);

#endif
