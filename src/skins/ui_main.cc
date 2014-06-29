/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team.
 *
 *  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/audstrings.h>
#include <libaudgui/libaudgui.h>

#include "actions-mainwin.h"
#include "actions-playlist.h"
#include "dnd.h"
#include "menus.h"
#include "plugin.h"
#include "skins_cfg.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_main_evlisteners.h"
#include "ui_playlist.h"
#include "ui_skinned_button.h"
#include "ui_skinned_horizontal_slider.h"
#include "ui_skinned_menurow.h"
#include "ui_skinned_monostereo.h"
#include "ui_skinned_number.h"
#include "ui_skinned_playlist.h"
#include "ui_skinned_playstatus.h"
#include "ui_skinned_textbox.h"
#include "ui_skinned_window.h"
#include "ui_vis.h"
#include "util.h"
#include "view.h"

#define SEEK_THRESHOLD 200 /* milliseconds */
#define SEEK_TIMEOUT 100 /* milliseconds */
#define SEEK_SPEED 50 /* milliseconds per pixel */

GtkWidget *mainwin = nullptr;

static int balance;
static int seek_source = 0, seek_start, seek_time;

static GtkWidget *mainwin_menubtn, *mainwin_minimize, *mainwin_shade, *mainwin_close;
static GtkWidget *mainwin_shaded_menubtn, *mainwin_shaded_minimize, *mainwin_shaded_shade, *mainwin_shaded_close;

static GtkWidget *mainwin_rew, *mainwin_fwd;
static GtkWidget *mainwin_eject;
static GtkWidget *mainwin_play, *mainwin_pause, *mainwin_stop;

GtkWidget *mainwin_shuffle, *mainwin_repeat;
GtkWidget *mainwin_eq, *mainwin_pl;

GtkWidget *mainwin_info;
GtkWidget *mainwin_stime_min, *mainwin_stime_sec;

static GtkWidget *mainwin_rate_text, *mainwin_freq_text, *mainwin_othertext;

GtkWidget *mainwin_playstatus;

GtkWidget *mainwin_minus_num, *mainwin_10min_num, *mainwin_min_num;
GtkWidget *mainwin_10sec_num, *mainwin_sec_num;

GtkWidget *mainwin_vis;
GtkWidget *mainwin_svis;

GtkWidget *mainwin_sposition = nullptr;

GtkWidget *mainwin_menurow;
static GtkWidget *mainwin_volume, *mainwin_balance;
GtkWidget *mainwin_position;

static GtkWidget *mainwin_monostereo;
static GtkWidget *mainwin_srew, *mainwin_splay, *mainwin_spause;
static GtkWidget *mainwin_sstop, *mainwin_sfwd, *mainwin_seject, *mainwin_about;

static gboolean mainwin_info_text_locked = FALSE;
static unsigned mainwin_volume_release_timeout = 0;

static void mainwin_position_motion_cb (void);
static void mainwin_position_release_cb (void);
static void mainwin_set_volume_diff (int diff);

static void format_time (char buf[7], int time, int length)
{
    gboolean zero = aud_get_bool (nullptr, "leading_zero");
    gboolean remaining = aud_get_bool ("skins", "show_remaining_time");

    if (remaining && length > 0)
    {
        time = (length - time) / 1000;

        if (time < 60)
            snprintf (buf, 7, zero ? "-00:%02d" : " -0:%02d", time);
        else if (time < 6000)
            snprintf (buf, 7, zero ? "%03d:%02d" : "%3d:%02d", -time / 60, time % 60);
        else
            snprintf (buf, 7, "%3d:%02d", -time / 3600, time / 60 % 60);
    }
    else
    {
        time /= 1000;

        if (time < 6000)
            snprintf (buf, 7, zero ? " %02d:%02d" : " %2d:%02d", time / 60, time % 60);
        else if (time < 60000)
            snprintf (buf, 7, "%3d:%02d", time / 60, time % 60);
        else
            snprintf (buf, 7, "%3d:%02d", time / 3600, time / 60 % 60);
    }
}

void mainwin_set_shape (void)
{
    int id = aud_get_bool ("skins", "player_shaded") ? SKIN_MASK_MAIN_SHADE : SKIN_MASK_MAIN;
    gtk_widget_shape_combine_mask (mainwin, active_skin->masks[id], 0, 0);
}

static void
mainwin_menubtn_cb(void)
{
    int x, y;
    gtk_window_get_position(GTK_WINDOW(mainwin), &x, &y);
    menu_popup (UI_MENU_MAIN, x + 6, y + MAINWIN_SHADED_HEIGHT, FALSE, FALSE, 1, GDK_CURRENT_TIME);
}

static void mainwin_minimize_cb (void)
{
    if (!mainwin)
        return;

    gtk_window_iconify(GTK_WINDOW(mainwin));
}

static void
mainwin_shade_toggle(void)
{
    view_set_player_shaded (! aud_get_bool ("skins", "player_shaded"));
}

static char *mainwin_tb_old_text = nullptr;

static void mainwin_lock_info_text (const char * text)
{
    if (mainwin_info_text_locked != TRUE)
        mainwin_tb_old_text = g_strdup
         (active_skin->properties.mainwin_othertext_is_status ?
         textbox_get_text (mainwin_othertext) : textbox_get_text (mainwin_info));

    mainwin_info_text_locked = TRUE;
    if (active_skin->properties.mainwin_othertext_is_status)
        textbox_set_text (mainwin_othertext, text);
    else
        textbox_set_text (mainwin_info, text);
}

static void mainwin_release_info_text (void)
{
    mainwin_info_text_locked = FALSE;

    if (mainwin_tb_old_text != nullptr)
    {
        if (active_skin->properties.mainwin_othertext_is_status)
            textbox_set_text (mainwin_othertext, mainwin_tb_old_text);
        else
            textbox_set_text (mainwin_info, mainwin_tb_old_text);
        g_free(mainwin_tb_old_text);
        mainwin_tb_old_text = nullptr;
    }
}

static void mainwin_set_info_text (const char * text)
{
    if (mainwin_info_text_locked)
    {
        g_free (mainwin_tb_old_text);
        mainwin_tb_old_text = g_strdup (text);
    }
    else
        textbox_set_text (mainwin_info, text);
}

static int status_message_source = 0;

static gboolean clear_status_message (void * unused)
{
    mainwin_release_info_text ();
    status_message_source = 0;
    return FALSE;
}

void mainwin_show_status_message (const char * message)
{
    if (status_message_source)
        g_source_remove (status_message_source);

    mainwin_lock_info_text (message);
    status_message_source = g_timeout_add (1000, clear_status_message, nullptr);
}

static char *
make_mainwin_title(const char * title)
{
    if (title != nullptr)
        return g_strdup_printf(_("%s - Audacious"), title);
    else
        return g_strdup(_("Audacious"));
}

void
mainwin_set_song_title(const char * title)
{
    char *mainwin_title_text = make_mainwin_title(title);
    gtk_window_set_title(GTK_WINDOW(mainwin), mainwin_title_text);
    g_free(mainwin_title_text);

    mainwin_set_info_text (title ? title : "");
}

static void setup_widget (GtkWidget * widget, int x, int y, gboolean show)
{
    /* leave no-show-all widgets alone (they are shown/hidden elsewhere) */
    if (! gtk_widget_get_no_show_all (widget))
    {
        int width, height;

        /* use get_size_request(), not get_preferred_size() */
        /* get_preferred_size() will return 0x0 for hidden widgets */
        gtk_widget_get_size_request (widget, & width, & height);

        /* hide widgets that are outside the window boundary */
        if (x < 0 || x + width > active_skin->properties.mainwin_width ||
         y < 0 || y + height > active_skin->properties.mainwin_height)
            show = FALSE;

        gtk_widget_set_visible (widget, show);
    }

    window_move_widget (mainwin, FALSE, widget, x, y);
}

void mainwin_refresh_hints (void)
{
    const SkinProperties * p = & active_skin->properties;

    gtk_widget_set_visible (mainwin_menurow, p->mainwin_menurow_visible);
    gtk_widget_set_visible (mainwin_rate_text, p->mainwin_streaminfo_visible);
    gtk_widget_set_visible (mainwin_freq_text, p->mainwin_streaminfo_visible);
    gtk_widget_set_visible (mainwin_monostereo, p->mainwin_streaminfo_visible);

    textbox_set_width (mainwin_info, p->mainwin_text_width);

    setup_widget (mainwin_vis, p->mainwin_vis_x, p->mainwin_vis_y, p->mainwin_vis_visible);
    setup_widget (mainwin_info, p->mainwin_text_x, p->mainwin_text_y, p->mainwin_text_visible);
    setup_widget (mainwin_othertext, p->mainwin_infobar_x, p->mainwin_infobar_y, p->mainwin_othertext_visible);

    setup_widget (mainwin_minus_num, p->mainwin_number_0_x, p->mainwin_number_0_y, TRUE);
    setup_widget (mainwin_10min_num, p->mainwin_number_1_x, p->mainwin_number_1_y, TRUE);
    setup_widget (mainwin_min_num, p->mainwin_number_2_x, p->mainwin_number_2_y, TRUE);
    setup_widget (mainwin_10sec_num, p->mainwin_number_3_x, p->mainwin_number_3_y, TRUE);
    setup_widget (mainwin_sec_num, p->mainwin_number_4_x, p->mainwin_number_4_y, TRUE);
    setup_widget (mainwin_position, p->mainwin_position_x, p->mainwin_position_y, TRUE);

    setup_widget (mainwin_playstatus, p->mainwin_playstatus_x, p->mainwin_playstatus_y, TRUE);
    setup_widget (mainwin_volume, p->mainwin_volume_x, p->mainwin_volume_y, TRUE);
    setup_widget (mainwin_balance, p->mainwin_balance_x, p->mainwin_balance_y, TRUE);
    setup_widget (mainwin_rew, p->mainwin_previous_x, p->mainwin_previous_y, TRUE);
    setup_widget (mainwin_play, p->mainwin_play_x, p->mainwin_play_y, TRUE);
    setup_widget (mainwin_pause, p->mainwin_pause_x, p->mainwin_pause_y, TRUE);
    setup_widget (mainwin_stop, p->mainwin_stop_x, p->mainwin_stop_y, TRUE);
    setup_widget (mainwin_fwd, p->mainwin_next_x, p->mainwin_next_y, TRUE);
    setup_widget (mainwin_eject, p->mainwin_eject_x, p->mainwin_eject_y, TRUE);
    setup_widget (mainwin_eq, p->mainwin_eqbutton_x, p->mainwin_eqbutton_y, TRUE);
    setup_widget (mainwin_pl, p->mainwin_plbutton_x, p->mainwin_plbutton_y, TRUE);
    setup_widget (mainwin_shuffle, p->mainwin_shuffle_x, p->mainwin_shuffle_y, TRUE);
    setup_widget (mainwin_repeat, p->mainwin_repeat_x, p->mainwin_repeat_y, TRUE);
    setup_widget (mainwin_about, p->mainwin_about_x, p->mainwin_about_y, TRUE);
    setup_widget (mainwin_minimize, p->mainwin_minimize_x, p->mainwin_minimize_y, TRUE);
    setup_widget (mainwin_shade, p->mainwin_shade_x, p->mainwin_shade_y, TRUE);
    setup_widget (mainwin_close, p->mainwin_close_x, p->mainwin_close_y, TRUE);

    if (aud_get_bool ("skins", "player_shaded"))
        window_set_size (mainwin, MAINWIN_SHADED_WIDTH, MAINWIN_SHADED_HEIGHT);
    else
        window_set_size (mainwin, p->mainwin_width, p->mainwin_height);
}

/* note that the song info is not translated since it is displayed using
 * the skinned bitmap font, which supports only the English alphabet */
void mainwin_set_song_info (int bitrate, int samplerate, int channels)
{
    char scratch[32];
    int length;

    if (bitrate > 0)
    {
        if (bitrate < 1000000)
            snprintf (scratch, sizeof scratch, "%3d", bitrate / 1000);
        else
            snprintf (scratch, sizeof scratch, "%2dH", bitrate / 100000);

        textbox_set_text (mainwin_rate_text, scratch);
    }
    else
        textbox_set_text (mainwin_rate_text, "");

    if (samplerate > 0)
    {
        snprintf (scratch, sizeof scratch, "%2d", samplerate / 1000);
        textbox_set_text (mainwin_freq_text, scratch);
    }
    else
        textbox_set_text (mainwin_freq_text, "");

    ui_skinned_monostereo_set_num_channels (mainwin_monostereo, channels);

    if (bitrate > 0)
        snprintf (scratch, sizeof scratch, "%d kbps", bitrate / 1000);
    else
        scratch[0] = 0;

    if (samplerate > 0)
    {
        length = strlen (scratch);
        snprintf (scratch + length, sizeof scratch - length, "%s%d kHz", length ?
         ", " : "", samplerate / 1000);
    }

    if (channels > 0)
    {
        length = strlen (scratch);
        snprintf (scratch + length, sizeof scratch - length, "%s%s", length ?
         ", " : "", channels > 2 ? "surround" : channels > 1 ? "stereo" : "mono");
    }

    textbox_set_text (mainwin_othertext, scratch);
}

void
mainwin_clear_song_info(void)
{
    mainwin_set_song_title (nullptr);

    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);

    gtk_widget_hide (mainwin_minus_num);
    gtk_widget_hide (mainwin_10min_num);
    gtk_widget_hide (mainwin_min_num);
    gtk_widget_hide (mainwin_10sec_num);
    gtk_widget_hide (mainwin_sec_num);
    gtk_widget_hide (mainwin_stime_min);
    gtk_widget_hide (mainwin_stime_sec);
    gtk_widget_hide (mainwin_position);
    gtk_widget_hide (mainwin_sposition);

    hslider_set_pressed (mainwin_position, FALSE);
    hslider_set_pressed (mainwin_sposition, FALSE);

    /* clear sampling parameter displays */
    textbox_set_text (mainwin_rate_text, "   ");
    textbox_set_text (mainwin_freq_text, "  ");
    ui_skinned_monostereo_set_num_channels(mainwin_monostereo, 0);
    textbox_set_text (mainwin_othertext, "");

    if (mainwin_playstatus != nullptr)
        ui_skinned_playstatus_set_status(mainwin_playstatus, STATUS_STOP);

    playlistwin_hide_timer();
}

void
mainwin_disable_seekbar(void)
{
    if (!mainwin)
        return;

    gtk_widget_hide(mainwin_position);
    gtk_widget_hide(mainwin_sposition);
}

static void mainwin_scrolled (GtkWidget * widget, GdkEventScroll * event, void *
 unused)
{
    switch (event->direction) {
        case GDK_SCROLL_UP:
            mainwin_set_volume_diff (5);
            break;
        case GDK_SCROLL_DOWN:
            mainwin_set_volume_diff (-5);
            break;
        case GDK_SCROLL_LEFT:
            aud_drct_seek (aud_drct_get_time () - 5000);
            break;
        case GDK_SCROLL_RIGHT:
            aud_drct_seek (aud_drct_get_time () + 5000);
            break;
        default:
            break;
    }
}

static gboolean
mainwin_mouse_button_press(GtkWidget * widget,
                           GdkEventButton * event,
                           void * callback_data)
{
    if (event->button == 1 && event->type == GDK_2BUTTON_PRESS &&
     event->window == gtk_widget_get_window (widget) && event->y < 14)
    {
        mainwin_shade_toggle ();
        return TRUE;
    }

    if (event->button == 3)
    {
        menu_popup (UI_MENU_MAIN, event->x_root, event->y_root, FALSE, FALSE,
         event->button, event->time);
        return TRUE;
    }

    return FALSE;
}

static void mainwin_playback_rpress (GtkWidget * button, GdkEventButton * event)
{
    menu_popup (UI_MENU_PLAYBACK, event->x_root, event->y_root, FALSE, FALSE,
     event->button, event->time);
}

gboolean mainwin_keypress (GtkWidget * widget, GdkEventKey * event,
 void * unused)
{
    if (ui_skinned_playlist_key (playlistwin_list, event))
        return 1;

    switch (event->keyval)
    {
        case GDK_KEY_minus:
            mainwin_set_volume_diff (-5);
            break;
        case GDK_KEY_plus:
            mainwin_set_volume_diff (5);
            break;
        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
        case GDK_KEY_KP_7:
            aud_drct_seek (aud_drct_get_time () - 5000);
            break;
        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
        case GDK_KEY_KP_9:
            aud_drct_seek (aud_drct_get_time () + 5000);
            break;
        case GDK_KEY_KP_4:
            aud_drct_pl_prev ();
            break;
        case GDK_KEY_KP_6:
            aud_drct_pl_next ();
            break;
        case GDK_KEY_KP_Insert:
            audgui_jump_to_track ();
            break;
        case GDK_KEY_space:
            aud_drct_pause();
            break;
        case GDK_KEY_Tab: /* GtkUIManager does not handle tab, apparently. */
            if (event->state & GDK_SHIFT_MASK)
                action_playlist_prev ();
            else
                action_playlist_next ();

            break;
        case GDK_KEY_ISO_Left_Tab:
            action_playlist_prev ();
            break;
        default:
            return FALSE;
    }

    return TRUE;
}

/*
 * Rewritten 09/13/06:
 *
 * Remove all of this flaky iter/sourcelist/strsplit stuff.
 * All we care about is the filepath.
 *
 * We can figure this out and easily pass it to g_filename_from_uri().
 *   - nenolod
 */
void
mainwin_drag_data_received(GtkWidget * widget,
                           GdkDragContext * context,
                           int x,
                           int y,
                           GtkSelectionData * selection_data,
                           unsigned info,
                           unsigned time,
                           void * user_data)
{
    g_return_if_fail(selection_data != nullptr);

    const char * data = (const char *) gtk_selection_data_get_data
     (selection_data);
    g_return_if_fail (data);

    if (str_has_prefix_nocase (data, "file:///"))
    {
        if (str_has_suffix_nocase (data, ".wsz\r\n") || str_has_suffix_nocase
         (data, ".zip\r\n"))
        {
            on_skin_view_drag_data_received (0, context, x, y, selection_data, info, time, 0);
            return;
        }
    }

    audgui_urilist_open (data);
}

static int time_now (void)
{
    struct timeval tv;
    gettimeofday (& tv, nullptr);
    return (tv.tv_sec % (24 * 3600) * 1000 + tv.tv_usec / 1000);
}

static int time_diff (int a, int b)
{
    if (a > 18 * 3600 * 1000 && b < 6 * 3600 * 1000) /* detect midnight */
        b += 24 * 3600 * 1000;
    return (b > a) ? b - a : 0;
}

static gboolean seek_timeout (void * rewind)
{
    if (! aud_drct_get_playing ())
    {
        seek_source = 0;
        return FALSE;
    }

    int held = time_diff (seek_time, time_now ());
    if (held < SEEK_THRESHOLD)
        return TRUE;

    int position;
    if (GPOINTER_TO_INT (rewind))
        position = seek_start - held / SEEK_SPEED;
    else
        position = seek_start + held / SEEK_SPEED;

    position = aud::clamp (position, 0, 219);
    hslider_set_pos (mainwin_position, position);
    mainwin_position_motion_cb ();

    return TRUE;
}

static gboolean seek_press (GtkWidget * widget, GdkEventButton * event,
 gboolean rewind)
{
    if (event->button != 1 || seek_source)
        return FALSE;

    seek_start = hslider_get_pos (mainwin_position);
    seek_time = time_now ();
    seek_source = g_timeout_add (SEEK_TIMEOUT, seek_timeout, GINT_TO_POINTER
     (rewind));
    return FALSE;
}

static gboolean seek_release (GtkWidget * widget, GdkEventButton * event,
 gboolean rewind)
{
    if (event->button != 1 || ! seek_source)
        return FALSE;

    if (! aud_drct_get_playing () || time_diff (seek_time, time_now ()) <
     SEEK_THRESHOLD)
    {
        if (rewind)
            aud_drct_pl_prev ();
        else
            aud_drct_pl_next ();
    }
    else
        mainwin_position_release_cb ();

    g_source_remove (seek_source);
    seek_source = 0;
    return FALSE;
}

static void mainwin_rew_press (GtkWidget * button, GdkEventButton * event)
 {seek_press (button, event, TRUE); }
static void mainwin_rew_release (GtkWidget * button, GdkEventButton * event)
 {seek_release (button, event, TRUE); }
static void mainwin_fwd_press (GtkWidget * button, GdkEventButton * event)
 {seek_press (button, event, FALSE); }
static void mainwin_fwd_release (GtkWidget * button, GdkEventButton * event)
 {seek_release (button, event, FALSE); }

static void mainwin_shuffle_cb (GtkWidget * button, GdkEventButton * event)
 {aud_set_bool (nullptr, "shuffle", button_get_active (button)); }
static void mainwin_repeat_cb (GtkWidget * button, GdkEventButton * event)
 {aud_set_bool (nullptr, "repeat", button_get_active (button)); }
static void mainwin_eq_cb (GtkWidget * button, GdkEventButton * event)
 {view_set_show_equalizer (button_get_active (button)); }
static void mainwin_pl_cb (GtkWidget * button, GdkEventButton * event)
 {view_set_show_playlist (button_get_active (button)); }

static void mainwin_spos_set_knob (void)
{
    int pos = hslider_get_pos (mainwin_sposition);

    int x;
    if (pos < 6)
        x = 17;
    else if (pos < 9)
        x = 20;
    else
        x = 23;

    hslider_set_knob (mainwin_sposition, x, 36, x, 36);
}

static void mainwin_spos_motion_cb (void)
{
    mainwin_spos_set_knob ();

    int pos = hslider_get_pos (mainwin_sposition);
    int length = aud_drct_get_length ();
    int time = (pos - 1) * length / 12;

    char buf[7];
    format_time (buf, time, length);

    textbox_set_text (mainwin_stime_min, buf);
    textbox_set_text (mainwin_stime_sec, buf + 4);
}

static void mainwin_spos_release_cb (void)
{
    mainwin_spos_set_knob ();

    int pos = hslider_get_pos (mainwin_sposition);
    aud_drct_seek (aud_drct_get_length () * (pos - 1) / 12);
}

static void mainwin_position_motion_cb (void)
{
    int length = aud_drct_get_length () / 1000;
    int pos = hslider_get_pos (mainwin_position);
    int time = pos * length / 219;

    char * seek_msg = g_strdup_printf (_("Seek to %d:%-2.2d / %d:%-2.2d"), time
     / 60, time % 60, length / 60, length % 60);
    mainwin_lock_info_text(seek_msg);
    g_free(seek_msg);
}

static void mainwin_position_release_cb (void)
{
    int length = aud_drct_get_length ();
    int pos = hslider_get_pos (mainwin_position);
    int time = (int64_t) pos * length / 219;

    aud_drct_seek(time);
    mainwin_release_info_text();
}

void
mainwin_adjust_volume_motion(int v)
{
    char *volume_msg;

    volume_msg = g_strdup_printf(_("Volume: %d%%"), v);
    mainwin_lock_info_text(volume_msg);
    g_free(volume_msg);

    aud_drct_set_volume_main (v);
    aud_drct_set_volume_balance (balance);
}

void
mainwin_adjust_volume_release(void)
{
    mainwin_release_info_text();
}

void
mainwin_adjust_balance_motion(int b)
{
    char *balance_msg;

    balance = b;
    aud_drct_set_volume_balance (b);

    if (b < 0)
        balance_msg = g_strdup_printf(_("Balance: %d%% left"), -b);
    else if (b == 0)
        balance_msg = g_strdup_printf(_("Balance: center"));
    else
        balance_msg = g_strdup_printf(_("Balance: %d%% right"), b);

    mainwin_lock_info_text(balance_msg);
    g_free(balance_msg);
}

void
mainwin_adjust_balance_release(void)
{
    mainwin_release_info_text();
}

static void mainwin_volume_set_frame (void)
{
    int pos = hslider_get_pos (mainwin_volume);
    int frame = (pos * 27 + 25) / 51;
    hslider_set_frame (mainwin_volume, 0, 15 * frame);
}

void mainwin_set_volume_slider (int percent)
{
    hslider_set_pos (mainwin_volume, (percent * 51 + 50) / 100);
    mainwin_volume_set_frame ();
}

static void mainwin_volume_motion_cb (void)
{
    mainwin_volume_set_frame ();
    int pos = hslider_get_pos (mainwin_volume);
    int vol = (pos * 100 + 25) / 51;

    mainwin_adjust_volume_motion(vol);
    equalizerwin_set_volume_slider(vol);
}

static void mainwin_volume_release_cb (void)
{
    mainwin_volume_set_frame ();
    mainwin_adjust_volume_release();
}

static void mainwin_balance_set_frame (void)
{
    int pos = hslider_get_pos (mainwin_balance);
    int frame = (abs (pos - 12) * 27 + 6) / 12;
    hslider_set_frame (mainwin_balance, 9, 15 * frame);
}

void mainwin_set_balance_slider (int percent)
{
    if (percent > 0)
        hslider_set_pos (mainwin_balance, 12 + (percent * 12 + 50) / 100);
    else
        hslider_set_pos (mainwin_balance, 12 + (percent * 12 - 50) / 100);

    mainwin_balance_set_frame ();
}

static void mainwin_balance_motion_cb (void)
{
    mainwin_balance_set_frame ();
    int pos = hslider_get_pos (mainwin_balance);

    int bal;
    if (pos > 12)
        bal = ((pos - 12) * 100 + 6) / 12;
    else
        bal = ((pos - 12) * 100 - 6) / 12;

    mainwin_adjust_balance_motion(bal);
    equalizerwin_set_balance_slider(bal);
}

static void mainwin_balance_release_cb (void)
{
    mainwin_balance_set_frame ();
    mainwin_adjust_volume_release();
}

static void mainwin_set_volume_diff (int diff)
{
    int vol;

    aud_drct_get_volume_main (& vol);
    vol = aud::clamp (vol + diff, 0, 100);
    mainwin_adjust_volume_motion(vol);
    mainwin_set_volume_slider(vol);
    equalizerwin_set_volume_slider(vol);

    if (mainwin_volume_release_timeout)
        g_source_remove(mainwin_volume_release_timeout);
    mainwin_volume_release_timeout =
        g_timeout_add(700, (GSourceFunc)(mainwin_volume_release_cb), nullptr);
}

void mainwin_mr_change (MenuRowItem i)
{
    switch (i) {
        case MENUROW_OPTIONS:
            mainwin_lock_info_text(_("Options Menu"));
            break;
        case MENUROW_ALWAYS:
            if (aud_get_bool ("skins", "always_on_top"))
                mainwin_lock_info_text(_("Disable 'Always On Top'"));
            else
                mainwin_lock_info_text(_("Enable 'Always On Top'"));
            break;
        case MENUROW_FILEINFOBOX:
            mainwin_lock_info_text(_("File Info Box"));
            break;
        default:
            break;
    }
}

void mainwin_mr_release (MenuRowItem i, GdkEventButton * event)
{
    switch (i) {
        case MENUROW_OPTIONS:
            menu_popup (UI_MENU_VIEW, event->x_root, event->y_root, FALSE, FALSE, 1, event->time);
            break;
        case MENUROW_ALWAYS:
            view_set_on_top (! aud_get_bool ("skins", "always_on_top"));
            break;
        case MENUROW_FILEINFOBOX:
            audgui_infowin_show_current ();
            break;
        default:
            break;
    }

    mainwin_release_info_text();
}

gboolean
change_timer_mode_cb(GtkWidget *widget, GdkEventButton *event)
{
    if (event->button == 1)
        view_set_show_remaining (! aud_get_bool ("skins", "show_remaining_time"));
    else if (event->button == 3)
        return FALSE;

    return TRUE;
}

static gboolean mainwin_info_button_press (GtkWidget * widget, GdkEventButton *
 event)
{
    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        menu_popup (UI_MENU_PLAYBACK, event->x_root, event->y_root, FALSE,
         FALSE, event->button, event->time);
        return TRUE;
    }

    if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    {
        audgui_infowin_show_current ();
        return TRUE;
    }

    return FALSE;
}

static void
mainwin_create_widgets(void)
{
    mainwin_menubtn = button_new (9, 9, 0, 0, 0, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, FALSE, mainwin_menubtn, 6, 3);
    button_on_release (mainwin_menubtn, (ButtonCB) mainwin_menubtn_cb);

    mainwin_minimize = button_new (9, 9, 9, 0, 9, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, FALSE, mainwin_minimize, 244, 3);
    button_on_release (mainwin_minimize, (ButtonCB) mainwin_minimize_cb);

    mainwin_shade = button_new (9, 9, 0, 18, 9, 18, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, FALSE, mainwin_shade, 254, 3);
    button_on_release (mainwin_shade, (ButtonCB) mainwin_shade_toggle);

    mainwin_close = button_new (9, 9, 18, 0, 18, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, FALSE, mainwin_close, 264, 3);
    button_on_release (mainwin_close, (ButtonCB) handle_window_close);

    mainwin_rew = button_new (23, 18, 0, 0, 0, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    window_put_widget (mainwin, FALSE, mainwin_rew, 16, 88);
    button_on_press (mainwin_rew, mainwin_rew_press);
    button_on_release (mainwin_rew, mainwin_rew_release);
    button_on_rpress (mainwin_rew, mainwin_playback_rpress);

    mainwin_fwd = button_new (22, 18, 92, 0, 92, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    window_put_widget (mainwin, FALSE, mainwin_fwd, 108, 88);
    button_on_press (mainwin_fwd, mainwin_fwd_press);
    button_on_release (mainwin_fwd, mainwin_fwd_release);
    button_on_rpress (mainwin_fwd, mainwin_playback_rpress);

    mainwin_play = button_new (23, 18, 23, 0, 23, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    window_put_widget (mainwin, FALSE, mainwin_play, 39, 88);
    button_on_release (mainwin_play, (ButtonCB) aud_drct_play);
    button_on_rpress (mainwin_play, mainwin_playback_rpress);

    mainwin_pause = button_new (23, 18, 46, 0, 46, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    window_put_widget (mainwin, FALSE, mainwin_pause, 62, 88);
    button_on_release (mainwin_pause, (ButtonCB) aud_drct_pause);
    button_on_rpress (mainwin_pause, mainwin_playback_rpress);

    mainwin_stop = button_new (23, 18, 69, 0, 69, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    window_put_widget (mainwin, FALSE, mainwin_stop, 85, 88);
    button_on_release (mainwin_stop, (ButtonCB) aud_drct_stop);
    button_on_rpress (mainwin_stop, mainwin_playback_rpress);

    mainwin_eject = button_new (22, 16, 114, 0, 114, 16, SKIN_CBUTTONS, SKIN_CBUTTONS);
    window_put_widget (mainwin, FALSE, mainwin_eject, 136, 89);
    button_on_release (mainwin_eject, (ButtonCB) action_play_file);

    mainwin_shuffle = button_new_toggle (46, 15, 28, 0, 28, 15, 28, 30, 28, 45, SKIN_SHUFREP, SKIN_SHUFREP);
    window_put_widget (mainwin, FALSE, mainwin_shuffle, 164, 89);
    button_set_active (mainwin_shuffle, aud_get_bool (nullptr, "shuffle"));
    button_on_release (mainwin_shuffle, mainwin_shuffle_cb);

    mainwin_repeat = button_new_toggle (28, 15, 0, 0, 0, 15, 0, 30, 0, 45, SKIN_SHUFREP, SKIN_SHUFREP);
    window_put_widget (mainwin, FALSE, mainwin_repeat, 210, 89);
    button_set_active (mainwin_repeat, aud_get_bool (nullptr, "repeat"));
    button_on_release (mainwin_repeat, mainwin_repeat_cb);

    mainwin_eq = button_new_toggle (23, 12, 0, 61, 46, 61, 0, 73, 46, 73, SKIN_SHUFREP, SKIN_SHUFREP);
    window_put_widget (mainwin, FALSE, mainwin_eq, 219, 58);
    button_on_release (mainwin_eq, mainwin_eq_cb);

    mainwin_pl = button_new_toggle (23, 12, 23, 61, 69, 61, 23, 73, 69, 73, SKIN_SHUFREP, SKIN_SHUFREP);
    window_put_widget (mainwin, FALSE, mainwin_pl, 242, 58);
    button_on_release (mainwin_pl, mainwin_pl_cb);

    String font = aud_get_str ("skins", "mainwin_font");
    mainwin_info = textbox_new (153, "", config.mainwin_use_bitmapfont ? nullptr :
     (const char *) font, config.autoscroll);

    window_put_widget (mainwin, FALSE, mainwin_info, 112, 27);
    g_signal_connect (mainwin_info, "button-press-event", (GCallback)
     mainwin_info_button_press, nullptr);

    mainwin_othertext = textbox_new (153, "", nullptr, FALSE);
    window_put_widget (mainwin, FALSE, mainwin_othertext, 112, 43);

    mainwin_rate_text = textbox_new (15, "", nullptr, FALSE);
    window_put_widget (mainwin, FALSE, mainwin_rate_text, 111, 43);

    mainwin_freq_text = textbox_new (10, "", nullptr, FALSE);
    window_put_widget (mainwin, FALSE, mainwin_freq_text, 156, 43);

    mainwin_menurow = ui_skinned_menurow_new ();
    window_put_widget (mainwin, FALSE, mainwin_menurow, 10, 22);

    mainwin_volume = hslider_new (0, 51, SKIN_VOLUME, 68, 13, 0, 0, 14, 11, 15, 422, 0, 422);
    window_put_widget (mainwin, FALSE, mainwin_volume, 107, 57);
    hslider_on_motion (mainwin_volume, mainwin_volume_motion_cb);
    hslider_on_release (mainwin_volume, mainwin_volume_release_cb);

    mainwin_balance = hslider_new (0, 24, SKIN_BALANCE, 38, 13, 9, 0, 14, 11, 15, 422, 0, 422);
    window_put_widget (mainwin, FALSE, mainwin_balance, 177, 57);
    hslider_on_motion (mainwin_balance, mainwin_balance_motion_cb);
    hslider_on_release (mainwin_balance, mainwin_balance_release_cb);

    mainwin_monostereo = ui_skinned_monostereo_new ();
    window_put_widget (mainwin, FALSE, mainwin_monostereo, 212, 41);

    mainwin_playstatus = ui_skinned_playstatus_new ();
    window_put_widget (mainwin, FALSE, mainwin_playstatus, 24, 28);

    mainwin_minus_num = ui_skinned_number_new ();
    window_put_widget (mainwin, FALSE, mainwin_minus_num, 36, 26);
    g_signal_connect(mainwin_minus_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), nullptr);

    mainwin_10min_num = ui_skinned_number_new ();
    window_put_widget (mainwin, FALSE, mainwin_10min_num, 48, 26);
    g_signal_connect(mainwin_10min_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), nullptr);

    mainwin_min_num = ui_skinned_number_new ();
    window_put_widget (mainwin, FALSE, mainwin_min_num, 60, 26);
    g_signal_connect(mainwin_min_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), nullptr);

    mainwin_10sec_num = ui_skinned_number_new ();
    window_put_widget (mainwin, FALSE, mainwin_10sec_num, 78, 26);
    g_signal_connect(mainwin_10sec_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), nullptr);

    mainwin_sec_num = ui_skinned_number_new ();
    window_put_widget (mainwin, FALSE, mainwin_sec_num, 90, 26);
    g_signal_connect(mainwin_sec_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), nullptr);

    mainwin_about = button_new_small (20, 25);
    window_put_widget (mainwin, FALSE, mainwin_about, 247, 83);
    button_on_release (mainwin_about, (ButtonCB) audgui_show_about_window);

    mainwin_vis = ui_vis_new ();
    window_put_widget (mainwin, FALSE, mainwin_vis, 24, 43);

    mainwin_position = hslider_new (0, 219, SKIN_POSBAR, 248, 10, 0, 0, 29, 10, 248, 0, 278, 0);
    window_put_widget (mainwin, FALSE, mainwin_position, 16, 72);
    hslider_on_motion (mainwin_position, mainwin_position_motion_cb);
    hslider_on_release (mainwin_position, mainwin_position_release_cb);

    /* shaded */

    mainwin_shaded_menubtn = button_new (9, 9, 0, 0, 0, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, TRUE, mainwin_shaded_menubtn, 6, 3);
    button_on_release (mainwin_shaded_menubtn, (ButtonCB) mainwin_menubtn_cb);

    mainwin_shaded_minimize = button_new (9, 9, 9, 0, 9, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, TRUE, mainwin_shaded_minimize, 244, 3);
    button_on_release (mainwin_shaded_minimize, (ButtonCB) mainwin_minimize_cb);

    mainwin_shaded_shade = button_new (9, 9, 0, 27, 9, 27, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, TRUE, mainwin_shaded_shade, 254, 3);
    button_on_release (mainwin_shaded_shade, (ButtonCB) mainwin_shade_toggle);

    mainwin_shaded_close = button_new (9, 9, 18, 0, 18, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, TRUE, mainwin_shaded_close, 264, 3);
    button_on_release (mainwin_shaded_close, (ButtonCB) handle_window_close);

    mainwin_srew = button_new_small (8, 7);
    window_put_widget (mainwin, TRUE, mainwin_srew, 169, 4);
    button_on_release (mainwin_srew, (ButtonCB) aud_drct_pl_prev);

    mainwin_splay = button_new_small (10, 7);
    window_put_widget (mainwin, TRUE, mainwin_splay, 177, 4);
    button_on_release (mainwin_splay, (ButtonCB) aud_drct_play);

    mainwin_spause = button_new_small (10, 7);
    window_put_widget (mainwin, TRUE, mainwin_spause, 187, 4);
    button_on_release (mainwin_spause, (ButtonCB) aud_drct_pause);

    mainwin_sstop = button_new_small (9, 7);
    window_put_widget (mainwin, TRUE, mainwin_sstop, 197, 4);
    button_on_release (mainwin_sstop, (ButtonCB) aud_drct_stop);

    mainwin_sfwd = button_new_small (8, 7);
    window_put_widget (mainwin, TRUE, mainwin_sfwd, 206, 4);
    button_on_release (mainwin_sfwd, (ButtonCB) aud_drct_pl_next);

    mainwin_seject = button_new_small (9, 7);
    window_put_widget (mainwin, TRUE, mainwin_seject, 216, 4);
    button_on_release (mainwin_seject, (ButtonCB) action_play_file);

    mainwin_svis = ui_svis_new ();
    window_put_widget (mainwin, TRUE, mainwin_svis, 79, 5);

    mainwin_sposition = hslider_new (1, 13, SKIN_TITLEBAR, 17, 7, 0, 36, 3, 7, 17, 36, 17, 36);
    window_put_widget (mainwin, TRUE, mainwin_sposition, 226, 4);
    hslider_on_motion (mainwin_sposition, mainwin_spos_motion_cb);
    hslider_on_release (mainwin_sposition, mainwin_spos_release_cb);

    mainwin_stime_min = textbox_new (15, "", nullptr, FALSE);
    window_put_widget (mainwin, TRUE, mainwin_stime_min, 130, 4);

    mainwin_stime_sec = textbox_new (10, "", nullptr, FALSE);
    window_put_widget (mainwin, TRUE, mainwin_stime_sec, 147, 4);

    g_signal_connect(mainwin_stime_min, "button-press-event", G_CALLBACK(change_timer_mode_cb), nullptr);
    g_signal_connect(mainwin_stime_sec, "button-press-event", G_CALLBACK(change_timer_mode_cb), nullptr);
}

static void show_widgets (void)
{
    gtk_widget_set_no_show_all (mainwin_minus_num, TRUE);
    gtk_widget_set_no_show_all (mainwin_10min_num, TRUE);
    gtk_widget_set_no_show_all (mainwin_min_num, TRUE);
    gtk_widget_set_no_show_all (mainwin_10sec_num, TRUE);
    gtk_widget_set_no_show_all (mainwin_sec_num, TRUE);
    gtk_widget_set_no_show_all (mainwin_stime_min, TRUE);
    gtk_widget_set_no_show_all (mainwin_stime_sec, TRUE);
    gtk_widget_set_no_show_all (mainwin_position, TRUE);
    gtk_widget_set_no_show_all (mainwin_sposition, TRUE);

    window_set_shaded (mainwin, aud_get_bool ("skins", "player_shaded"));
    window_show_all (mainwin);
}

static gboolean state_cb (GtkWidget * widget, GdkEventWindowState * event,
 void * unused)
{
    if (event->changed_mask & GDK_WINDOW_STATE_STICKY)
        view_set_sticky (!! (event->new_window_state & GDK_WINDOW_STATE_STICKY));

    if (event->changed_mask & GDK_WINDOW_STATE_ABOVE)
        view_set_on_top (!! (event->new_window_state & GDK_WINDOW_STATE_ABOVE));

    return TRUE;
}

static void mainwin_draw (GtkWidget * window, cairo_t * cr)
{
    gboolean shaded = aud_get_bool ("skins", "player_shaded");
    int width = shaded ? MAINWIN_SHADED_WIDTH : active_skin->properties.mainwin_width;
    int height = shaded ? MAINWIN_SHADED_HEIGHT : active_skin->properties.mainwin_height;

    skin_draw_pixbuf (cr, SKIN_MAIN, 0, 0, 0, 0, width, height);
    skin_draw_mainwin_titlebar (cr, shaded, TRUE);
}

static void
mainwin_create_window(void)
{
    gboolean shaded = aud_get_bool ("skins", "player_shaded");
    int width = shaded ? MAINWIN_SHADED_WIDTH : active_skin->properties.mainwin_width;
    int height = shaded ? MAINWIN_SHADED_HEIGHT : active_skin->properties.mainwin_height;

    mainwin = window_new (& config.player_x, & config.player_y, width, height,
     TRUE, shaded, mainwin_draw);

    gtk_window_set_title(GTK_WINDOW(mainwin), _("Audacious"));

    g_signal_connect(mainwin, "button_press_event",
                     G_CALLBACK(mainwin_mouse_button_press), nullptr);
    g_signal_connect(mainwin, "scroll_event",
                     G_CALLBACK(mainwin_scrolled), nullptr);

    drag_dest_set(mainwin);
    g_signal_connect ((GObject *) mainwin, "drag-data-received", (GCallback)
     mainwin_drag_data_received, 0);

    g_signal_connect(mainwin, "key_press_event",
                     G_CALLBACK(mainwin_keypress), nullptr);

    ui_main_evlistener_init();

    g_signal_connect ((GObject *) mainwin, "window-state-event", (GCallback) state_cb, nullptr);
    g_signal_connect ((GObject *) mainwin, "delete-event", (GCallback) handle_window_close, nullptr);
}

void mainwin_unhook (void)
{
    if (seek_source != 0)
    {
        g_source_remove (seek_source);
        seek_source = 0;
    }

    ui_main_evlistener_dissociate ();
    start_stop_visual (TRUE);
}

void
mainwin_create(void)
{
    mainwin_create_window();

    gtk_window_add_accel_group ((GtkWindow *) mainwin, menu_get_accel_group ());

    mainwin_create_widgets();
    show_widgets ();
}

static void mainwin_update_volume (void)
{
    int volume, balance;

    aud_drct_get_volume_main (& volume);
    aud_drct_get_volume_balance (& balance);
    mainwin_set_volume_slider (volume);
    mainwin_set_balance_slider (balance);
    equalizerwin_set_volume_slider (volume);
    equalizerwin_set_balance_slider (balance);
}

static void mainwin_update_time_display (int time, int length)
{
    char scratch[7];
    format_time (scratch, time, length);

    ui_skinned_number_set (mainwin_minus_num, scratch[0]);
    ui_skinned_number_set (mainwin_10min_num, scratch[1]);
    ui_skinned_number_set (mainwin_min_num, scratch[2]);
    ui_skinned_number_set (mainwin_10sec_num, scratch[4]);
    ui_skinned_number_set (mainwin_sec_num, scratch[5]);

    if (! hslider_get_pressed (mainwin_sposition))
    {
        textbox_set_text (mainwin_stime_min, scratch);
        textbox_set_text (mainwin_stime_sec, scratch + 4);
    }

    playlistwin_set_time (scratch, scratch + 4);
}

static void mainwin_update_time_slider (int time, int length)
{
    gtk_widget_set_visible (mainwin_position, length > 0);
    gtk_widget_set_visible (mainwin_sposition, length > 0);

    if (length > 0 && seek_source == 0)
    {
        if (time < length)
        {
            hslider_set_pos (mainwin_position, time * (int64_t) 219 / length);
            hslider_set_pos (mainwin_sposition, 1 + time * (int64_t) 12 / length);
        }
        else
        {
            hslider_set_pos (mainwin_position, 219);
            hslider_set_pos (mainwin_sposition, 13);
        }

        mainwin_spos_set_knob ();
    }
}

void mainwin_update_song_info (void)
{
    mainwin_update_volume ();

    if (! aud_drct_get_playing ())
        return;

    int time = 0, length = 0;
    if (aud_drct_get_ready ())
    {
        time = aud_drct_get_time ();
        length = aud_drct_get_length ();
    }

    mainwin_update_time_display (time, length);
    mainwin_update_time_slider (time, length);
}

/* actionentries actions */

void action_play_file (void)
{
    audgui_run_filebrowser(TRUE); /* TRUE = PLAY_BUTTON */
}

void action_play_location (void)
{
    audgui_show_add_url_window (TRUE);
}

void action_ab_set (void)
{
    if (aud_drct_get_length () > 0)
    {
        int a, b;
        aud_drct_get_ab_repeat (& a, & b);

        if (a < 0 || b >= 0)
        {
            a = aud_drct_get_time ();
            b = -1;
            mainwin_show_status_message (_("Repeat point A set."));
        }
        else
        {
            b = aud_drct_get_time ();
            mainwin_show_status_message (_("Repeat point B set."));
        }

        aud_drct_set_ab_repeat (a, b);
    }
}

void action_ab_clear (void)
{
    mainwin_show_status_message (_("Repeat points cleared."));
    aud_drct_set_ab_repeat (-1, -1);
}
