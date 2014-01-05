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

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <libaudcore/hook.h>

#include "skins_cfg.h"
#include "ui_main.h"
#include "ui_main_evlisteners.h"
#include "ui_skin.h"
#include "ui_skinned_button.h"
#include "ui_skinned_playstatus.h"
#include "ui_vis.h"
#include "util.h"

static void title_change (void)
{
    if (aud_drct_get_ready ())
    {
        gchar * title = aud_drct_get_title ();
        mainwin_set_song_title (title);
        str_unref (title);
    }
    else
        mainwin_set_song_title ("Buffering ...");
}

static void info_change (void)
{
    gint bitrate = 0, samplerate = 0, channels = 0;

    if (aud_drct_get_ready ())
        aud_drct_get_info (& bitrate, & samplerate, & channels);

    mainwin_set_song_info (bitrate, samplerate, channels);
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

    if (aud_drct_get_ready () && aud_drct_get_length () > 0)
    {
        gtk_widget_show (mainwin_position);
        gtk_widget_show (mainwin_sposition);
    }

    ui_skinned_playstatus_set_status(mainwin_playstatus, STATUS_PLAY);

    title_change ();
    info_change ();
}

static void
ui_main_evlistener_playback_stop(gpointer hook_data, gpointer user_data)
{
    mainwin_clear_song_info ();
}

static void repeat_toggled (void * data, void * user)
{
    button_set_active (mainwin_repeat, aud_get_bool (NULL, "repeat"));
}

static void shuffle_toggled (void * data, void * user)
{
    button_set_active (mainwin_shuffle, aud_get_bool (NULL, "shuffle"));
}

static void no_advance_toggled (void * data, void * user)
{
    if (aud_get_bool (NULL, "no_playlist_advance"))
        mainwin_show_status_message (_("Single mode."));
    else
        mainwin_show_status_message (_("Playlist mode."));
}

static void stop_after_song_toggled (void * hook_data, void * user_data)
{
    if (aud_get_bool (NULL, "stop_after_current_song"))
        mainwin_show_status_message (_("Stopping after song."));
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

static void vis_clear_cb (void)
{
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

static void render_mono_pcm (const gfloat * pcm)
{
    guchar data[512];

    if (config.vis_type != VIS_SCOPE)
        return;

    for (gint i = 0; i < 75; i ++)
    {
        /* the signal is amplified by 2x */
        /* output values are in the range 0 to 16 */
        gint val = 8 + roundf (16 * pcm[i * 512 / 75]);
        data[i] = CLAMP (val, 0, 16);
    }

    if (aud_get_bool ("skins", "player_shaded"))
        ui_svis_timeout_func (mainwin_svis, data);
    else
        ui_vis_timeout_func (mainwin_vis, data);
}

/* calculate peak dB level, where 1 is 0 dB */
static gfloat calc_peak_level (const gfloat * pcm, gint step)
{
    gfloat peak = 0.0001; /* must be > 0, or level = -inf */

    for (gint i = 0; i < 512; i ++)
    {
        peak = MAX (peak, * pcm);
        pcm += step;
    }

    return 20 * log10 (peak);
}

static void render_multi_pcm (const gfloat * pcm, gint channels)
{
    /* "VU meter" */
    if (config.vis_type != VIS_VOICEPRINT || ! aud_get_bool ("skins", "player_shaded"))
        return;

    guchar data[512];

    gint level = 38 + calc_peak_level (pcm, channels);
    data[0] = CLAMP (level, 0, 38);

    if (channels >= 2)
    {
        level = 38 + calc_peak_level (pcm + 1, channels);
        data[1] = CLAMP (level, 0, 38);
    }
    else
        data[1] = data[0];

    ui_svis_timeout_func (mainwin_svis, data);
}

/* convert linear frequency graph to logarithmic one */
static void make_log_graph (const gfloat * freq, gint bands, gint db_range, gint
 int_range, guchar * graph)
{
    static gint last_bands = 0;
    static gfloat * xscale = NULL;

    /* conversion table for the x-axis */
    if (bands != last_bands)
    {
        xscale = g_realloc (xscale, sizeof (gfloat) * (bands + 1));
        for (int i = 0; i <= bands; i ++)
            xscale[i] = powf (256, (float) i / bands) - 0.5f;

        last_bands = bands;
    }

    for (gint i = 0; i < bands; i ++)
    {
        /* sum up values in freq array between xscale[i] and xscale[i + 1],
           including fractional parts */
        gint a = ceilf (xscale[i]);
        gint b = floorf (xscale[i + 1]);
        gfloat sum = 0;

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
        sum *= (gfloat) bands / 12;

        /* convert to dB */
        gfloat val = 20 * log10f (sum);

        /* scale (-db_range, 0.0) to (0.0, int_range) */
        val = (1 + val / db_range) * int_range;

        graph[i] = CLAMP (val, 0, int_range);
    }
}

static void render_freq (const gfloat * freq)
{
    bool_t shaded = aud_get_bool ("skins", "player_shaded");

    guchar data[512];

    if (config.vis_type == VIS_ANALYZER)
    {
        if (config.analyzer_type == ANALYZER_BARS)
        {
            if (shaded)
                make_log_graph (freq, 13, 40, 8, data);
            else
                make_log_graph (freq, 19, 40, 16, data);
        }
        else
        {
            if (shaded)
                make_log_graph (freq, 37, 40, 8, data);
            else
                make_log_graph (freq, 75, 40, 16, data);
        }
    }
    else if (config.vis_type == VIS_VOICEPRINT && ! shaded)
        make_log_graph (freq, 17, 40, 255, data);
    else
        return;

    if (shaded)
        ui_svis_timeout_func (mainwin_svis, data);
    else
        ui_vis_timeout_func (mainwin_vis, data);
}

void
ui_main_evlistener_init(void)
{
    hook_associate("hide seekbar", ui_main_evlistener_hide_seekbar, NULL);
    hook_associate("playback begin", ui_main_evlistener_playback_begin, NULL);
    hook_associate("playback ready", ui_main_evlistener_playback_begin, NULL);
    hook_associate("playback stop", ui_main_evlistener_playback_stop, NULL);
    hook_associate("playback pause", ui_main_evlistener_playback_pause, NULL);
    hook_associate("playback unpause", ui_main_evlistener_playback_unpause, NULL);
    hook_associate ("title change", (HookFunction) title_change, NULL);
    hook_associate ("info change", (HookFunction) info_change, NULL);

    hook_associate("playback seek", (HookFunction) mainwin_update_song_info, NULL);

    hook_associate ("set repeat", repeat_toggled, NULL);
    hook_associate ("set shuffle", shuffle_toggled, NULL);
    hook_associate ("set no_playlist_advance", no_advance_toggled, NULL);
    hook_associate ("set stop_after_current_song", stop_after_song_toggled, NULL);
}

void
ui_main_evlistener_dissociate(void)
{
    hook_dissociate("hide seekbar", ui_main_evlistener_hide_seekbar);
    hook_dissociate("playback begin", ui_main_evlistener_playback_begin);
    hook_dissociate("playback ready", ui_main_evlistener_playback_begin);
    hook_dissociate("playback stop", ui_main_evlistener_playback_stop);
    hook_dissociate("playback pause", ui_main_evlistener_playback_pause);
    hook_dissociate("playback unpause", ui_main_evlistener_playback_unpause);
    hook_dissociate ("title change", (HookFunction) title_change);
    hook_dissociate ("info change", (HookFunction) info_change);

    hook_dissociate("playback seek", (HookFunction) mainwin_update_song_info);

    hook_dissociate ("set repeat", repeat_toggled);
    hook_dissociate ("set shuffle", shuffle_toggled);
    hook_dissociate ("set no_playlist_advance", no_advance_toggled);
    hook_dissociate ("set stop_after_current_song", stop_after_song_toggled);
}

void start_stop_visual (gboolean exiting)
{
    static char started = 0;

    if (config.vis_type != VIS_OFF && ! exiting && gtk_widget_get_visible (mainwin))
    {
        if (! started)
        {
            aud_vis_func_add (AUD_VIS_TYPE_CLEAR, (VisFunc) vis_clear_cb);
            aud_vis_func_add (AUD_VIS_TYPE_MONO_PCM, (VisFunc) render_mono_pcm);
            aud_vis_func_add (AUD_VIS_TYPE_MULTI_PCM, (VisFunc) render_multi_pcm);
            aud_vis_func_add (AUD_VIS_TYPE_FREQ, (VisFunc) render_freq);
            started = 1;
        }
    }
    else
    {
        if (started)
        {
            aud_vis_func_remove ((VisFunc) vis_clear_cb);
            aud_vis_func_remove ((VisFunc) render_mono_pcm);
            aud_vis_func_remove ((VisFunc) render_multi_pcm);
            aud_vis_func_remove ((VisFunc) render_freq);
            started = 0;
        }
    }
}
