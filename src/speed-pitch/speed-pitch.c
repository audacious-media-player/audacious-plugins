/*
 * Speed and Pitch effect plugin for Audacious
 * Copyright 2012 John Lindgren
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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <samplerate.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>

/* The general idea of the speed change algorithm is to divide the input signal
 * into pieces, spaced at a time interval A, using a cosine-shaped window
 * function.  The pieces are then reassembled by adding them together again,
 * spaced at another time interval B.  By varying the ratio A:B, we change the
 * speed of the audio.  To get better results at the two ends of a song, we add
 * a short period of silence (half the width of the cosine window, to be exact)
 * to each end of the input signal beforehand and afterwards trim the same
 * amount from each end of the output signal. */

#define FREQ    10
#define OVERLAP  3

#define CFGSECT "speed-pitch"
#define MINSPEED 0.5
#define MAXSPEED 2.0
#define MINPITCH 0.5
#define MAXPITCH 2.0

#define BYTES(frames) ((frames) * curchans * sizeof (float))
#define OFFSET(buf,frames) ((buf) + (frames) * curchans)

typedef struct {
    float * mem;
    int size, len;
} Buffer;

static int curchans, currate;
static SRC_STATE * srcstate;
static int outstep, width;
static double * cosine;
static Buffer in, out;
static int trim, written;
static bool_t ending;

static void bufgrow (Buffer * b, int len)
{
    if (len > b->size)
    {
        b->mem = realloc (b->mem, BYTES (len));
        b->size = len;
    }

    if (len > b->len)
    {
        memset (OFFSET (b->mem, b->len), 0, BYTES (len - b->len));
        b->len = len;
    }
}

static void bufcut (Buffer * b, int len)
{
    memmove (b->mem, OFFSET (b->mem, len), BYTES (b->len - len));
    b->len -= len;
}

static void bufadd (Buffer * b, float * data, int len, double ratio)
{
    int oldlen = b->len;
    int max = len * ratio + 100;
    bufgrow (b, oldlen + max);

    SRC_DATA d = {
     .data_in = data,
     .input_frames = len,
     .data_out = OFFSET (b->mem, oldlen),
     .output_frames = max,
     .src_ratio = ratio};

    src_process (srcstate, & d);
    b->len = oldlen + d.output_frames_gen;
}

static void speed_flush (void)
{
    src_reset (srcstate);

    in.len = 0;
    out.len = 0;

    /* Add silence to the beginning of the input signal. */
    bufgrow (& in, width / 2);

    trim = width / 2;
    written = 0;
    ending = FALSE;
}

static void speed_start (int * chans, int * rate)
{
    curchans = * chans;
    currate = * rate;

    if (srcstate)
        src_delete (srcstate);

    srcstate = src_new (SRC_LINEAR, curchans, NULL);

    /* Calculate the width of the cosine window and the spacing interval for
     * output. */
    outstep = currate / FREQ;
    width = outstep * OVERLAP;

    /* Generate the cosine window, scaled vertically to compensate for the
     * overlap of the reassembled pieces of audio. */
    cosine = realloc (cosine, width * sizeof (double));
    for (int i = 0; i < width; i ++)
        cosine[i] = (1.0 - cos (2.0 * M_PI * i / width)) / OVERLAP;

    speed_flush ();
}

static void speed_process (float * * data, int * samples)
{
    double pitch = aud_get_double (CFGSECT, "pitch");
    double speed = aud_get_double (CFGSECT, "speed");

    /* Remove audio that has already been played from the output buffer. */
    bufcut (& out, written);

    /* Copy the passed audio to the input buffer, scaled to adjust pitch. */
    bufadd (& in, * data, * samples / curchans, 1.0 / pitch);

    /* If we are ending, add silence to the end of the input signal. */
    if (ending)
        bufgrow (& in, in.len + width / 2);

    /* Calculate the spacing interval for input. */
    int instep = round (outstep * speed / pitch);

    /* Run the speed change algorithm. */
    int src = 0;
    int dst = 0;

    while (src + MAX (width, instep) <= in.len)
    {
        bufgrow (& out, dst + width);
        out.len = dst + width;

        for (int i = 0; i < width; i ++)
        for (int c = 0; c < curchans; c ++)
            OFFSET (out.mem, dst + i)[c] += OFFSET (in.mem, src + i)[c] * cosine[i];

        src += instep;
        dst += outstep;
    }

    /* Remove processed audio from the input buffer. */
    bufcut (& in, src);

    /* Trim silence from the beginning of the output buffer. */
    if (trim > 0)
    {
        int cut = MIN (trim, dst);
        bufcut (& out, cut);
        dst -= cut;
        trim -= cut;
    }

    /* If we are ending, return all of the output buffer except the silence that
     * we trim from the end of it. */
    if (ending)
        dst = out.len - width / 2;

    /* Return processed audio in the output buffer and mark it to be removed on
     * the next call. */
    * data = out.mem;
    * samples = dst * curchans;
    written = dst;
}

static void speed_finish (float * * data, int * samples)
{
    /* Ignore the second "end of playlist" call since we are not in a state to
     * handle it properly. */
    if (! ending)
    {
        ending = TRUE;
        speed_process (data, samples);
    }
}

static int speed_adjust_delay (int delay)
{
    /* Not sample-accurate, but should be a decent estimate. */
    double speed = aud_get_double (CFGSECT, "speed");
    return delay * speed + width * 1000 / currate;
}

static const char * const speed_defaults[] = {
 "speed", "1",
 "pitch", "1",
 NULL};

static const PreferencesWidget speed_widgets[] = {
 {WIDGET_LABEL, N_("<b>Speed and Pitch</b>")},
 {WIDGET_SPIN_BTN, N_("Speed:"),
  .cfg_type = VALUE_FLOAT, .csect = CFGSECT, .cname = "speed",
  .data = {.spin_btn = {MINSPEED, MAXSPEED, 0.05}}},
 {WIDGET_SPIN_BTN, N_("Pitch:"),
  .cfg_type = VALUE_FLOAT, .csect = CFGSECT, .cname = "pitch",
  .data = {.spin_btn = {MINPITCH, MAXPITCH, 0.05}}}};

static const PluginPreferences speed_prefs = {
 .widgets = speed_widgets,
 .n_widgets = ARRAY_LEN (speed_widgets)};

static bool_t speed_init (void)
{
    aud_config_set_defaults (CFGSECT, speed_defaults);
    return TRUE;
}

static void speed_cleanup (void)
{
    if (srcstate)
        src_delete (srcstate);

    srcstate = NULL;

    free (cosine);
    cosine = NULL;

    free (in.mem);
    in.mem = NULL;
    in.size = 0;

    free (out.mem);
    out.mem = NULL;
    out.size = 0;
}

AUD_EFFECT_PLUGIN
(
    .name = N_("Speed and Pitch"),
    .domain = PACKAGE,
    .prefs = & speed_prefs,
    .init = speed_init,
    .cleanup = speed_cleanup,
    .start = speed_start,
    .process = speed_process,
    .flush = speed_flush,
    .finish = speed_finish,
    .adjust_delay = speed_adjust_delay,
    .preserves_format = TRUE
)
