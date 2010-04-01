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

#include <glib.h>
#include <math.h>

#include <audacious/plugin.h>
#include <audacious/input.h>

#include "skins_cfg.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_manager.h"
#include "ui_playlist.h"
#include "ui_skinned_playstatus.h"
#include "ui_skinned_textbox.h"
#include "ui_skinned_window.h"
#include "util.h"

static void ui_main_evlistener_title_change (void * data, void * user_data)
{
    gchar * title;

    /* may be called asynchronously */
    if (! audacious_drct_get_playing ())
        return;

    title = aud_playback_get_title ();
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

    if (audacious_drct_get_length () > 0)
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

    mainwin_enable_status_message (FALSE);
    check_set (toggleaction_group_others, "stop after current song", FALSE);
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

static void info_change (void * hook_data, void * user_data)
{
    gint bitrate, samplerate, channels;

    /* may be called asynchronously */
    if (! audacious_drct_get_playing ())
        return;

    audacious_drct_get_info (& bitrate, & samplerate, & channels);
    mainwin_set_song_info (bitrate, samplerate, channels);
}

static void
ui_main_evlistener_mainwin_set_always_on_top(gpointer hook_data, gpointer user_data)
{
    gboolean *ontop = (gboolean*)hook_data;
    mainwin_set_always_on_top(*ontop);
}

static void
ui_main_evlistener_mainwin_show(gpointer hook_data, gpointer user_data)
{
    gboolean show = GPOINTER_TO_INT(hook_data);
    mainwin_show(show);
}

static void
ui_main_evlistener_equalizerwin_show(gpointer hook_data, gpointer user_data)
{
    gboolean *show = (gboolean*)hook_data;
    equalizerwin_show(*show);
}

static void
ui_main_evlistener_visualization_timeout(gpointer hook_data, gpointer user_data)
{
    VisNode *vis = (VisNode*) hook_data;

    guint8 intern_vis_data[512];
    gint16 mono_freq[2][256];
    gboolean mono_freq_calced = FALSE;
    gint16 mono_pcm[2][512], stereo_pcm[2][512];
    gboolean mono_pcm_calced = FALSE, stereo_pcm_calced = FALSE;
    gint i;

    if (! vis || config.vis_type == VIS_OFF)
        return;

    if (config.vis_type == VIS_ANALYZER) {
            /* Spectrum analyzer */
            /* 76 values */
            const gint long_xscale[] =
                { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                17, 18,
                19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
                34,
                35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
                50, 51,
                52, 53, 54, 55, 56, 57, 58, 61, 66, 71, 76, 81, 87, 93,
                100, 107,
                114, 122, 131, 140, 150, 161, 172, 184, 255
            };
            /* 20 values */
            const int short_xscale[] =
                { 0, 1, 2, 3, 4, 5, 6, 7, 8, 11, 15, 20, 27,
                36, 47, 62, 82, 107, 141, 184, 255
            };
            const double y_scale = 3.60673760222;   /* 20.0 / log(256) */
            const int *xscale;
            gint j, y, max;

            if (!mono_freq_calced)
                aud_calc_mono_freq(mono_freq, vis->data, vis->nch);

            memset(intern_vis_data, 0, 75);

            if (config.analyzer_type == ANALYZER_BARS) {
                if (config.player_shaded) {
                    max = 13;
                }
                else {
                    max = 19;
                }
                xscale = short_xscale;
            }
            else {
                if (config.player_shaded) {
                    max = 37;
                }
                else {
                    max = 75;
                }
                xscale = long_xscale;
            }

            for (i = 0; i < max; i++) {
                for (j = xscale[i], y = 0; j < xscale[i + 1]; j++) {
                    if (mono_freq[0][j] > y)
                        y = mono_freq[0][j];
                }
                y >>= 7;
                if (y != 0) {
                    intern_vis_data[i] = log(y) * y_scale;
                    if (intern_vis_data[i] > 15)
                        intern_vis_data[i] = 15;
                }
                else
                    intern_vis_data[i] = 0;
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
            intern_vis_data[0] = (vu * 37) >> 15;
            if (intern_vis_data[0] > 37)
                intern_vis_data[0] = 37;
            if (vis->nch == 2) {
                vu = 0;
                for (i = 0; i < 512; i++) {
                    val = abs(stereo_pcm[1][i]);
                    if (val > vu)
                        vu = val;
                }
                intern_vis_data[1] = (vu * 37) >> 15;
                if (intern_vis_data[1] > 37)
                    intern_vis_data[1] = 37;
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
    aud_hook_associate("title change", ui_main_evlistener_title_change, NULL);
    aud_hook_associate("hide seekbar", ui_main_evlistener_hide_seekbar, NULL);
    aud_hook_associate("playback begin", ui_main_evlistener_playback_begin, NULL);
    aud_hook_associate("playback stop", ui_main_evlistener_playback_stop, NULL);
    aud_hook_associate("playback pause", ui_main_evlistener_playback_pause, NULL);
    aud_hook_associate("playback unpause", ui_main_evlistener_playback_unpause, NULL);
    aud_hook_associate("playback play file", ui_main_evlistener_playback_play_file, NULL);
    aud_hook_associate ("visualization clear", vis_clear_cb, NULL);
    aud_hook_associate ("info change", info_change, NULL);
    aud_hook_associate("mainwin set always on top", ui_main_evlistener_mainwin_set_always_on_top, NULL);
    aud_hook_associate("mainwin show", ui_main_evlistener_mainwin_show, NULL);
    aud_hook_associate("equalizerwin show", ui_main_evlistener_equalizerwin_show, NULL);

    aud_hook_associate("playback seek", (HookFunction) mainwin_update_song_info, NULL);
}

void
ui_main_evlistener_dissociate(void)
{
    aud_hook_dissociate("title change", ui_main_evlistener_title_change);
    aud_hook_dissociate("hide seekbar", ui_main_evlistener_hide_seekbar);
    aud_hook_dissociate("playback begin", ui_main_evlistener_playback_begin);
    aud_hook_dissociate("playback stop", ui_main_evlistener_playback_stop);
    aud_hook_dissociate("playback pause", ui_main_evlistener_playback_pause);
    aud_hook_dissociate("playback unpause", ui_main_evlistener_playback_unpause);
    aud_hook_dissociate("playback play file", ui_main_evlistener_playback_play_file);
    aud_hook_dissociate ("visualization clear", vis_clear_cb);
    aud_hook_dissociate ("info change", info_change);
    aud_hook_dissociate("mainwin set always on top", ui_main_evlistener_mainwin_set_always_on_top);
    aud_hook_dissociate("mainwin show", ui_main_evlistener_mainwin_show);
    aud_hook_dissociate("equalizerwin show", ui_main_evlistener_equalizerwin_show);

    aud_hook_dissociate("playback seek", (HookFunction) mainwin_update_song_info);
}

void start_stop_visual (void)
{
    static char started = 0;

    if (config.player_visible && config.vis_type != VIS_OFF)
    {
        if (! started)
        {
            aud_hook_associate ("visualization timeout",
             ui_main_evlistener_visualization_timeout, 0);
            started = 1;
        }
    }
    else
    {
        if (started)
        {
            aud_hook_dissociate ("visualization timeout",
             ui_main_evlistener_visualization_timeout);
            started = 0;
        }
    }
}
