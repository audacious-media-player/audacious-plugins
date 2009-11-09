/*
 * ALSA Gapless Output Plugin for Audacious
 * Copyright 2009 John Lindgren
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

#ifndef AUDACIOUS_ALSA_GAPLESS_H
#define AUDACIOUS_ALSA_GAPLESS_H

#include <stdio.h>
#include <glib.h>
#include <alsa/asoundlib.h>
#include <audacious/plugin.h>

#define ERROR(...) fprintf (stderr, "alsa-gapless: " __VA_ARGS__)

#if 0
#define DEBUG(...) ERROR (__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#define CHECK(function, ...) \
do { \
    gint error = function (__VA_ARGS__); \
    if (error < 0) { \
        ERROR ("%s failed: %s.\n", #function, snd_strerror (error)); \
        goto FAILED; \
    } \
} while (0)

/* alsa.c */
extern GMutex * alsa_mutex;

OutputPluginInitStatus alsa_init (void);
void alsa_soft_init (void);
void alsa_cleanup (void);
gint alsa_open_audio (AFormat aud_format, gint rate, gint channels);
void alsa_close_audio (void);
void alsa_write_audio (void * data, gint length);
gint alsa_written_time (void);
gint alsa_output_time (void);
gint alsa_buffer_free (void);
gint alsa_buffer_playing (void);
void alsa_flush (gint time);
void alsa_pause (gshort pause);
void alsa_get_volume (gint * left, gint * right);
void alsa_set_volume (gint left, gint right);
void alsa_open_mixer (void);
void alsa_close_mixer (void);

/* config.c */
extern gchar * alsa_config_pcm, * alsa_config_mixer, * alsa_config_mixer_element;

void alsa_config_load (void);
void alsa_config_save (void);
void alsa_configure (void);

/* plugin.c */
void alsa_about (void);

#endif
