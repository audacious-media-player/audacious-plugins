/*
 * OSS4 Output Plugin for Audacious
 * Copyright 2010 Michał Lipski <tallica@o2.pl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef AUDACIOUS_OSS4_H
#define AUDACIOUS_OSS4_H

#include "config.h"

#include <stdio.h>
#include <glib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <soundcard.h>

#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <audacious/debug.h>

#define ERROR(...) fprintf(stderr, "OSS4: " __VA_ARGS__)

#ifdef DEBUG
    #define DEBUG_MSG \
    do { \
        oss_message = oss_describe_error(); \
        AUDDBG("%s", oss_message); \
        g_free(oss_message); \
    } while (0)
#else
    #define DEBUG_MSG do { } while (0)
#endif

#define ERROR_MSG \
do { \
    oss_message = oss_describe_error(); \
    ERROR("%s", oss_message); \
    g_free(oss_message); \
} while (0)

#define SHOW_ERROR_MSG \
do { \
    oss_message = oss_describe_error(); \
    oss_show_error(oss_message); \
    ERROR("%s", oss_message); \
    g_free(oss_message); \
} while (0)

#define DEFAULT_MIXER "/dev/mixer"
#define DEFAULT_DSP "/dev/dsp"

typedef struct
{
    gint fd;
    gint format;
    gint rate;
    gint channels;
    gint bits_per_sample;
} oss_data_t;

typedef struct
{
    gchar *device;
    gboolean use_alt_device;
    gchar *alt_device;
    gint volume;
    gboolean save_volume;
    gboolean cookedmode;
} oss_cfg_t;

extern oss_data_t *oss_data;
extern oss_cfg_t *oss_cfg;
extern gchar *oss_message;

/* oss.c */
gboolean oss_init(void);
void oss_cleanup(void);
gint oss_open_audio(gint aud_format, gint rate, gint channels);
void oss_close_audio(void);
void oss_write_audio(void *data, gint length);
void oss_drain(void);
gint oss_buffer_free(void);
void oss_set_written_time(gint time);
gint oss_written_time(void);
gint oss_output_time(void);
void oss_flush(gint time);
void oss_pause(gboolean pause);
void oss_get_volume(gint *left, gint *right);
void oss_set_volume(gint left, gint right);

/* configure.c */
void oss_config_load(void);
void oss_config_save(void);
void oss_configure(void);

/* plugin.c */
void oss_about(void);

/* utils.c */
gint oss_convert_aud_format(gint aud_format);
gchar *oss_format_to_text(gint format);
gint oss_format_to_bits(gint format);
gint oss_frames_to_bytes(gint frames);
gint oss_bytes_to_frames(gint bytes);
gint oss_calc_bitrate(void);
gchar *oss_describe_error(void);
gboolean oss_hardware_present(void);
void oss_show_error(const gchar *message);

#endif
