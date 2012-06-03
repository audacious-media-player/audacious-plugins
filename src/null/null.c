/*
 *  Copyright 2006 Christian Birchinger <joker@netswarm.net>
 *
 *  Based on the XMMS plugin:
 *  Copyright 2000 Håvard Kvålen <havardk@sol.no>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <glib.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>

#include "config.h"

static GTimer *timer;
static gulong offset_time, written;
static gint bps;
static gboolean paused, started;

static struct {
    gint format;
    gint frequency;
    gint channels;
} input_format;

static const gchar * const null_defaults[] = {
 "real_time", "TRUE",
 NULL};

#define ELAPSED_TIME (offset_time + g_timer_elapsed(timer, NULL) * 1000)

static gboolean null_init (void)
{
    aud_config_set_defaults("null", null_defaults);
    return TRUE;
}

static int null_open(gint fmt, int rate, int nch)
{
    offset_time = 0;
    written = 0;
    started = FALSE;
    paused = FALSE;
    input_format.format = fmt;
    input_format.frequency = rate;
    input_format.channels = nch;
    bps = rate * nch;
    switch (fmt)
    {
        case FMT_U8:
        case FMT_S8:
            break;
        default:
            bps <<= 1;
            break;
    }
    if (aud_get_bool("null", "real_time"))
        timer = g_timer_new();
    return 1;
}

static void null_write(void *ptr, int length)
{
    if (timer && !started)
    {
        g_timer_start(timer);
        started = TRUE;
    }

    written += length;
}

static void null_close(void)
{
    if (timer)
        g_timer_destroy(timer);
    timer = NULL;
}

static void null_flush(int time)
{
    offset_time = time;
    written = ((double)time * bps) / 1000;
    if (timer)
        g_timer_reset(timer);
}

static void null_pause (gboolean p)
{
    paused = p;
    if (!timer)
        return;

    if (paused)
        g_timer_stop(timer);
    else
    {
        offset_time += g_timer_elapsed(timer, NULL) * 1000;
        g_timer_start(timer);
    }
}

static int null_buffer_free(void)
{
    if (timer)
    {
        return 10240 - (written - (ELAPSED_TIME * bps) / 1000);
    }
    else
    {
        if (!paused)
            return 10000;
        else
            return 0;
    }
}

#if 0
static int null_playing(void)
{
    if (!timer)
        return FALSE;

    if ((gdouble)(written * 1000) / bps > ELAPSED_TIME)
        return TRUE;
    return FALSE;
}
#endif

static int null_get_written_time(void)
{
    if (!bps)
        return 0;
    return ((gint64)written * 1000) / bps;
}

static int null_get_output_time(void)
{
    if (!timer)
        return null_get_written_time();

    return ELAPSED_TIME;
}

static const char null_about[] =
 "Null Output Plugin\n\n"
 "By Christian Birchinger <joker@netswarm.net>\n"
 "Based on the XMMS plugin by Håvard Kvål <havardk@xmms.org>";

static const PreferencesWidget null_widgets[] = {
 {WIDGET_LABEL, N_("<b>Output</b>")},
 {WIDGET_CHK_BTN, N_("Run in real time"),
  .cfg_type = VALUE_BOOLEAN, .csect = "null", .cname = "real_time"}};

static const PluginPreferences null_prefs = {
 .widgets = null_widgets,
 .n_widgets = sizeof null_widgets / sizeof null_widgets[0]};

AUD_OUTPUT_PLUGIN
(
 .name = N_("No Output"),
 .domain = PACKAGE,
 .about_text = null_about,
 .prefs = & null_prefs,
 .probe_priority = 0,
 .init = null_init,
 .open_audio = null_open,
 .write_audio = null_write,
 .close_audio = null_close,
 .flush = null_flush,
 .pause = null_pause,
 .buffer_free = null_buffer_free,
 .output_time = null_get_output_time,
 .written_time = null_get_written_time
)
