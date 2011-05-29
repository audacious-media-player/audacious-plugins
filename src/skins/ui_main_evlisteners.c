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

static void
ui_main_evlistener_playback_play_file(gpointer hook_data, gpointer user_data)
{
    if (config.random_skin_on_play)
        skin_set_random_skin();
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

static void ui_main_evlistener_visualization_timeout (const VisNode * vis,
 void * user)
{
    guint8 intern_vis_data[512];
    gint16 mono_freq[2][256];
    gboolean mono_freq_calced = FALSE;
    gint16 mono_pcm[2][512], stereo_pcm[2][512];
    gboolean mono_pcm_calced = FALSE, stereo_pcm_calced = FALSE;
    gint i;

    if (! vis || config.vis_type == VIS_OFF)
        return;

    if (config.vis_type == VIS_ANALYZER)
    {
        /* logarithmic scale - 1 */
        const gfloat xscale13[14] = {0.00, 0.53, 1.35, 2.60, 4.51, 7.45, 12.0,
         18.8, 29.4, 45.6, 70.4, 108, 167, 256};
        const gfloat xscale19[20] = {0.00, 0.34, 0.79, 1.40, 2.22, 3.31, 4.77,
         6.72, 9.34, 12.9, 17.6, 23.8, 32.3, 43.6, 58.7, 78.9, 106, 142, 191,
         256};
        const gfloat xscale37[38] = {0.00, 0.16, 0.35, 0.57, 0.82, 1.12, 1.46,
         1.86, 2.32, 2.86, 3.48, 4.21, 5.05, 6.03, 7.16, 8.48, 10.0, 11.8, 13.9,
         16.3, 19.1, 22.3, 26.1, 30.5, 35.6, 41.5, 48.4, 56.4, 65.6, 76.4, 88.9,
         104, 120, 140, 163, 189, 220, 256};
        const gfloat xscale75[76] = {0.00, 0.08, 0.16, 0.25, 0.34, 0.45, 0.56,
         0.68, 0.81, 0.95, 1.10, 1.26, 1.43, 1.62, 1.82, 2.03, 2.27, 2.52, 2.79,
         3.08, 3.39, 3.73, 4.09, 4.48, 4.90, 5.36, 5.85, 6.37, 6.94, 7.55, 8.20,
         8.91, 9.67, 10.5, 11.4, 12.3, 13.3, 14.4, 15.6, 16.9, 18.3, 19.8, 21.4,
         23.1, 24.9, 26.9, 29.1, 31.4, 33.9, 36.5, 39.4, 42.5, 45.9, 49.5, 53.3,
         57.5, 62.0, 66.9, 72.0, 77.7, 83.7, 90.2, 97.2, 105, 113, 122, 131,
         141, 152, 164, 177, 190, 205, 221, 238, 256};

        const gfloat * xscale;
        gint bands, i;

        if (config.analyzer_type == ANALYZER_BARS)
        {
            if (config.player_shaded)
            {
                bands = 13;
                xscale = xscale13;
            }
            else
            {
                bands = 19;
                xscale = xscale19;
            }
        }
        else
        {
            if (config.player_shaded)
            {
                bands = 37;
                xscale = xscale37;
            }
            else
            {
                bands = 75;
                xscale = xscale75;
            }
        }

        aud_calc_mono_freq (mono_freq, vis->data, vis->nch);

        for (i = 0; i < bands; i ++)
        {
            gint a = ceil (xscale[i]);
            gint b = floor (xscale[i + 1]);
            gint n = 0;

            if (b < a)
                n += mono_freq[0][b] * (xscale[i + 1] - xscale[i]);
            else
            {
                if (a > 0)
                    n += mono_freq[0][a - 1] * (a - xscale[i]);
                for (; a < b; a ++)
                    n += mono_freq[0][a];
                if (b < 256)
                    n += mono_freq[0][b] * (xscale[i + 1] - b);
            }

            /* 30 dB range, amplified 10 dB + extra for narrower bands */
            /* 0.000235 == 1 / 13 / 32767 * 10^(40/20) */
            n = 10 * log10 (n * bands * 0.000235);
            intern_vis_data[i] = CLAMP (n, 0, 15);
        }
    }
    else if(config.vis_type == VIS_VOICEPRINT){
        if (config.player_shaded) {
            /* VU */
            gint vu, val;

            if (!stereo_pcm_calced)
                aud_calc_stereo_pcm(stereo_pcm, vis->data, vis->nch);
            vu = 0;
            for (i = 0; i < 512; i++) {
                val = abs(stereo_pcm[0][i]);
                if (val > vu)
                    vu = val;
            }
            intern_vis_data[0] = (vu * 38) >> 15;
            if (intern_vis_data[0] > 38)
                intern_vis_data[0] = 38;
            if (vis->nch == 2) {
                vu = 0;
                for (i = 0; i < 512; i++) {
                    val = abs(stereo_pcm[1][i]);
                    if (val > vu)
                        vu = val;
                }
                intern_vis_data[1] = (vu * 38) >> 15;
                if (intern_vis_data[1] > 38)
                    intern_vis_data[1] = 38;
            }
            else
                intern_vis_data[1] = intern_vis_data[0];
        }
        else { /*Voiceprint*/
            if (!mono_freq_calced)
                aud_calc_mono_freq(mono_freq, vis->data, vis->nch);
            memset(intern_vis_data, 0, 256);

            /* For the values [0-16] we use the frequency that's 3/2 as much.
               If we assume the 512 values calculated by calc_mono_freq to
               cover 0-22kHz linearly we get a range of
               [0-16] * 3/2 * 22000/512 = [0-1,031] Hz.
               Most stuff above that is harmonics and we want to utilize the
               16 samples we have to the max[tm]
               */
            for (i = 0; i < 50 ; i+=3){
                intern_vis_data[i/3] += (mono_freq[0][i/2] >> 5);

                /*Boost frequencies above 257Hz a little*/
                //if(i > 4 * 3)
                //  intern_vis_data[i/3] += 8;
            }
        }
    }
    else { /* (config.vis_type == VIS_SCOPE) */

        /* Oscilloscope */
        gint pos, step;

        if (!mono_pcm_calced)
            aud_calc_mono_pcm(mono_pcm, vis->data, vis->nch);

        step = (vis->length << 8) / 74;
        for (i = 0, pos = 0; i < 75; i++, pos += step) {
            intern_vis_data[i] = ((mono_pcm[0][pos >> 8]) >> 12) + 7;
            if (intern_vis_data[i] == 255)
                intern_vis_data[i] = 0;
            else if (intern_vis_data[i] > 12)
                intern_vis_data[i] = 12;
            /* Do not see the point of that? (comparison always false) -larne.
               if (intern_vis_data[i] < 0)
               intern_vis_data[i] = 0; */
        }
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
    hook_associate("playback play file", ui_main_evlistener_playback_play_file, NULL);
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
    hook_dissociate("playback play file", ui_main_evlistener_playback_play_file);
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
