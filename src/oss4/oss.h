/*
 * oss.h
 * Copyright 2010-2012 Michał Lipski
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

#ifndef AUDACIOUS_OSS4_H
#define AUDACIOUS_OSS4_H

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
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
    SPRINTF(oss_error_buf, "OSS4 error: "__VA_ARGS__); \
    aud_interface_show_error(oss_error_buf); \
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

#define N_ELEMENTS(arr) (sizeof (arr) / sizeof ((arr)[0]))

#define DEFAULT_MIXER "/dev/mixer"
#define DEFAULT_DSP "/dev/dsp"

typedef struct
{
    int fd;
    int format;
    int rate;
    int channels;
    int bits_per_sample;
} oss_data_t;

extern oss_data_t *oss_data;

/* oss.c */
bool_t oss_init(void);
void oss_cleanup(void);
int oss_open_audio(int aud_format, int rate, int channels);
void oss_close_audio(void);
void oss_write_audio(void *data, int length);
void oss_drain(void);
int oss_buffer_free(void);
int oss_output_time(void);
void oss_flush(int time);
void oss_pause(bool_t pause);
void oss_get_volume(int *left, int *right);
void oss_set_volume(int left, int right);

/* configure.c */
void oss_configure(void);

/* utils.c */
int oss_convert_aud_format(int aud_format);
char *oss_format_to_text(int format);
int oss_format_to_bits(int format);
int oss_frames_to_bytes(int frames);
int oss_bytes_to_frames(int bytes);
int oss_calc_bitrate(void);
char *oss_describe_error(void);
int oss_probe_for_adev(oss_sysinfo *sysinfo);
bool_t oss_hardware_present(void);

#endif
