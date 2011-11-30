/*
 * OSS4 Output Plugin for Audacious
 * Copyright 2010-2011 Michał Lipski <tallica@o2.pl>
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

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <glib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#else
#include <soundcard.h>
#endif

#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <audacious/debug.h>
#include <audacious/misc.h>

#define ERROR(...) \
do { \
    fprintf(stderr, "OSS4 %s:%d [%s]: ", __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, __VA_ARGS__); \
} while (0)

#define ERROR_NOISY(...) \
do { \
    oss_error(__VA_ARGS__); \
    ERROR(__VA_ARGS__); \
} while (0) \

#define DESCRIBE_ERROR ERROR("%s\n", oss_describe_error())
#define DESCRIBE_ERROR_NOISY ERROR_NOISY("%s\n", oss_describe_error())

#define CHECK(function, ...) \
do { \
    int error = function(__VA_ARGS__); \
    if (error < 0) { \
        DESCRIBE_ERROR; \
        goto FAILED; \
    } \
} while (0)

#define CHECK_NOISY(function, ...) \
do { \
    int error = function(__VA_ARGS__); \
    if (error < 0) { \
        DESCRIBE_ERROR_NOISY; \
        goto FAILED; \
    } \
} while (0)

#define CHECK_VAL(value, function, ...) \
do { \
    if (!(value)) { \
        function(__VA_ARGS__); \
        goto FAILED; \
    } \
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

extern oss_data_t *oss_data;

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
gint oss_probe_for_adev(oss_sysinfo *sysinfo);
gboolean oss_hardware_present(void);
gint oss_show_error(gpointer message);
void oss_error(const gchar *format, ...);

#endif
