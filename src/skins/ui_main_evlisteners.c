/*
 * Audacious
 * Copyright (c) 2006-2007 Audacious development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#include <math.h>

#include <audacious/audconfig.h>
#include <audacious/drct.h>
#include <audacious/misc.h>
#include <libaudcore/hook.h>

#include "skins_cfg.h"
#include "ui_main.h"
#include "ui_main_evlisteners.h"
#include "ui_manager.h"
#include "ui_skin.h"
#include "ui_skinned_playstatus.h"
#include "ui_vis.h"
#include "util.h"

static void ui_main_evlistener_title_change (void * data, void * user_data)
{
    gchar * title;

    /* may be called asynchronously */
    if (! aud_drct_get_playing ())
        return;

    title = aud_drct_get_title ();
    mainwin_set_song_title (title);
    g_free (title);
}

static void
ui_main_evlistener_hide_seekbar(gpointer hook_data, gpointer user_data)
{
    mainwin_disable_seekbar();
}

void ui_main_evlistener_playback_begin (void * hook_data, void * user_data)
{
    mainwin_disable_seekbar();
    mainwin_update_song_info();

    gtk_widget_show (mainwin_stime_min);
    gtk_widget_show (mainwin_stime_sec);
    gtk_widget_show (mainwin_minus_num);
    gtk_widget_show (mainwin_10min_num);
    gtk_widget_show (mainwin_min_num);
    gtk_widget_show (mainwin_10sec_num);
    gtk_widget_show (mainwin_sec_num);

    if (aud_drct_get_length () > 0)
    {
        gtk_widget_show (mainwin_position);
        gtk_widget_show (mainwin_sposition);
    }

    ui_skinned_playstatus_set_status(mainwin_playstatus, STATUS_PLAY);
    ui_main_evlistener_title_change (NULL, NULL);
}

static void
ui_main_evlistener_playback_stop(gpointer hook_data, gpointer user_data)
{
    mainwin_clear_song_info ();
}

static void stop_after_song_toggled (void * hook_data, void * user_data)
{
    mainwin_enable_status_message (FALSE);
    check_set (toggleaction_group_others, "stop after current song",
     aud_cfg->stopaftersong);
    mainwin_enable_status_message (TRUE);
}

void ui_main_evlistener_playback_pause (void * hook_data, void * user_data)
{
    ui_skinned_playstatus_set_status(mainwin_playstatus, STATUS_PAUSE);
}

static void
ui_main_evlistener_playback_unpause(gpointer hook_data, gpointer user_data)
{
    ui_skinned_playstatus_set_status(mainwin_playstatus, STATUS_PLAY);
}

static void vis_clear_cb (void * unused, void * another)
{
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void info_change (void)
{
    gint bitrate, samplerate, channels;

    /* may be called asynchronously */
    if (! aud_drct_get_playing ())
        return;

    aud_drct_get_info (& bitrate, & samplerate, & channels);
    mainwin_set_song_info (bitrate, samplerate, channels);
}

/* calculate peak dB level, where 32767 (max of 16-bit PCM range) is 0 dB */
static gfloat calc_peak_level (gint16 pcm[512])
{
    gint peak = 0;
    for (gint i = 0; i < 512; i ++)
        peak = MAX (peak, pcm[i]);

    return 20 * log10 (peak / 32767.0);
}

/* convert linear frequency graph to logarithmic one */
static void make_log_graph (gint16 freq[256], gint bands, gint db_range, gint
 int_range, guchar * graph)
{
    static gint last_bands = 0;
    static gfloat * xscale = NULL;

    /* conversion table for the x-axis */
    if (bands != last_bands)
    {
        xscale = g_realloc (xscale, sizeof (gfloat) * (bands + 1));
        for (gint i = 0; i <= bands; i ++)
            xscale[i] = powf (257, (gfloat) i / bands) - 1;

        last_bands = bands;
    }

    for (gint i = 0; i < bands; i ++)
    {
        /* sum up values in freq array between xscale[i] and xscale[i + 1],
           including fractional parts */
        gint a = ceilf (xscale[i]);
        gint b = floorf (xscale[i + 1]);
        gint sum = 0;

        if (b < a)
            sum += freq[b] * (xscale[i + 1] - xscale[i]);
        else
        {
            if (a > 0)
                sum += freq[a - 1] * (a - xscale[i]);
            for (; a < b; a ++)
                sum += freq[a];
            if (b < 256)
                sum += freq[b] * (xscale[i + 1] - b);
        }

        /* fudge factor to make the graph have the same overall height as a
           12-band one no matter how many bands there are */
        sum = sum * bands / 12;

        /* convert to dB, where 32767 (max of 16-bit PCM range) is 0 dB */
        gfloat val = 20 * log10 (sum / 32767.0);

        /* scale (-db_range, 0.0) to (0.0, int_range) */
        val = (1.0 + val / db_range) * int_range;

        graph[i] = CLAMP (val, 0, int_range);
    }
}

static void ui_main_evlistener_visualization_timeout (const VisNode * vis,
 void * user)
{
    guint8 intern_vis_data[512];
    gint16 mono_freq[2][256];
    gint16 mono_pcm[2][512], stereo_pcm[2][512];

    if (! vis || config.vis_type == VIS_OFF)
        return;

    switch (config.vis_type)
    {
    case VIS_ANALYZER:
        aud_calc_mono_freq (mono_freq, vis->data, vis->nch);

        if (config.analyzer_type == ANALYZER_BARS)
        {
            if (config.player_shaded)
                make_log_graph (mono_freq[0], 13, 40, 8, intern_vis_data);
            else
                make_log_graph (mono_freq[0], 19, 40, 16, intern_vis_data);
        }
        else
        {
            if (config.player_shaded)
                make_log_graph (mono_freq[0], 37, 40, 8, intern_vis_data);
            else
                make_log_graph (mono_freq[0], 75, 40, 16, intern_vis_data);
        }

        break;
    case VIS_VOICEPRINT:
        if (config.player_shaded) /* "VU meter" */
        {
            aud_calc_stereo_pcm (stereo_pcm, vis->data, vis->nch);

            gint level = 38 + calc_peak_level (stereo_pcm[0]);
            intern_vis_data[0] = CLAMP (level, 0, 38);

            if (vis->nch == 2)
            {
                level = 38 + calc_peak_level (stereo_pcm[1]);
                intern_vis_data[1] = CLAMP (level, 0, 38);
            }
            else
                intern_vis_data[1] = intern_vis_data[0];
        }
        else
        {
            aud_calc_mono_freq (mono_freq, vis->data, vis->nch);
            make_log_graph (mono_freq[0], 17, 40, 255, intern_vis_data);
        }

        break;
    default: /* VIS_SCOPE */
        aud_calc_mono_pcm (mono_pcm, vis->data, vis->nch);

        for (gint i = 0; i < 75; i ++)
        {
            /* the signal is amplified by 2x */
            /* output values are in the range 0 to 16 */
            gint val = 16384 + mono_pcm[0][i * vis->length / 75];
            val = CLAMP (val, 0, 32768);
            intern_vis_data[i] = (val + 1024) / 2048;
        }

        break;
    }

    if (config.player_shaded)
        ui_svis_timeout_func(mainwin_svis, intern_vis_data);
    else
        ui_vis_timeout_func(mainwin_vis, intern_vis_data);
}

void
ui_main_evlistener_init(void)
{
    hook_associate("title change", ui_main_evlistener_title_change, NULL);
    hook_associate("hide seekbar", ui_main_evlistener_hide_seekbar, NULL);
    hook_associate("playback begin", ui_main_evlistener_playback_begin, NULL);
    hook_associate("playback stop", ui_main_evlistener_playback_stop, NULL);
    hook_associate("playback pause", ui_main_evlistener_playback_pause, NULL);
    hook_associate("playback unpause", ui_main_evlistener_playback_unpause, NULL);
    hook_associate ("visualization clear", vis_clear_cb, NULL);
    hook_associate ("info change", (HookFunction) info_change, NULL);

    hook_associate("playback seek", (HookFunction) mainwin_update_song_info, NULL);
    hook_associate ("toggle stop after song", stop_after_song_toggled, NULL);
}

void
ui_main_evlistener_dissociate(void)
{
    hook_dissociate("title change", ui_main_evlistener_title_change);
    hook_dissociate("hide seekbar", ui_main_evlistener_hide_seekbar);
    hook_dissociate("playback begin", ui_main_evlistener_playback_begin);
    hook_dissociate("playback stop", ui_main_evlistener_playback_stop);
    hook_dissociate("playback pause", ui_main_evlistener_playback_pause);
    hook_dissociate("playback unpause", ui_main_evlistener_playback_unpause);
    hook_dissociate ("visualization clear", vis_clear_cb);
    hook_dissociate ("info change", (HookFunction) info_change);

    hook_dissociate("playback seek", (HookFunction) mainwin_update_song_info);
    hook_dissociate ("toggle stop after song", stop_after_song_toggled);
}

void start_stop_visual (gboolean exiting)
{
    static char started = 0;

    if (config.player_visible && config.vis_type != VIS_OFF && ! exiting)
    {
        if (! started)
        {
            aud_vis_runner_add_hook(ui_main_evlistener_visualization_timeout, NULL);
            started = 1;
        }
    }
    else
    {
        if (started)
        {
            aud_vis_runner_remove_hook(ui_main_evlistener_visualization_timeout);
            started = 0;
        }
    }
}
