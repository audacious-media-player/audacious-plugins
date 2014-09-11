/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009-2014 John Lindgren
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

#include <libaudcore/audstrings.h>
#include <libaudcore/interface.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#define ERROR AUDERR

#define ERROR_NOISY(...) do { \
    aud_ui_show_error (str_printf ("ALSA error: " __VA_ARGS__)); \
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
bool alsa_init ();
void alsa_cleanup ();
bool alsa_open_audio (int aud_format, int rate, int channels);
void alsa_close_audio ();
int alsa_buffer_free ();
void alsa_write_audio (void * data, int length);
void alsa_period_wait ();
void alsa_drain ();
int alsa_output_time ();
void alsa_flush (int time);
void alsa_pause (bool pause);
void alsa_open_mixer ();
void alsa_close_mixer ();
void alsa_get_volume (int * left, int * right);
void alsa_set_volume (int left, int right);

/* config.c */
void alsa_init_config ();

extern const PluginPreferences alsa_prefs;

#endif
