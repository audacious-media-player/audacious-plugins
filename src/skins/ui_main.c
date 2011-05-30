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

#include <gdk/gdkkeysyms.h>

#include <audacious/audconfig.h>
#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>

#include "actions-mainwin.h"
#include "actions-playlist.h"
#include "config.h"
#include "dnd.h"
#include "skins_cfg.h"
#include "ui_dock.h"
#include "ui_equalizer.h"
#include "ui_hints.h"
#include "ui_main.h"
#include "ui_main_evlisteners.h"
#include "ui_manager.h"
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

#define SEEK_THRESHOLD 200 /* milliseconds */
#define SEEK_TIMEOUT 100 /* milliseconds */
#define SEEK_SPEED 50 /* milliseconds per pixel */

GtkWidget *mainwin = NULL;

static gint balance;
static gint seek_source = 0, seek_start, seek_time;

static GtkWidget *mainwin_menubtn, *mainwin_minimize, *mainwin_shade, *mainwin_close;
static GtkWidget *mainwin_shaded_menubtn, *mainwin_shaded_minimize, *mainwin_shaded_shade, *mainwin_shaded_close;

static GtkWidget *mainwin_rew, *mainwin_fwd;
static GtkWidget *mainwin_eject;
static GtkWidget *mainwin_play, *mainwin_pause, *mainwin_stop;

static GtkWidget *mainwin_shuffle, *mainwin_repeat;
GtkWidget *mainwin_eq, *mainwin_pl;

GtkWidget *mainwin_info;
GtkWidget *mainwin_stime_min, *mainwin_stime_sec;

static GtkWidget *mainwin_rate_text, *mainwin_freq_text, *mainwin_othertext;

GtkWidget *mainwin_playstatus;

GtkWidget *mainwin_minus_num, *mainwin_10min_num, *mainwin_min_num;
GtkWidget *mainwin_10sec_num, *mainwin_sec_num;

GtkWidget *mainwin_vis;
GtkWidget *mainwin_svis;

GtkWidget *mainwin_sposition = NULL;

static GtkWidget *mainwin_menurow;
static GtkWidget *mainwin_volume, *mainwin_balance;
GtkWidget *mainwin_position;

static GtkWidget *mainwin_monostereo;
static GtkWidget *mainwin_srew, *mainwin_splay, *mainwin_spause;
static GtkWidget *mainwin_sstop, *mainwin_sfwd, *mainwin_seject, *mainwin_about;

static gboolean mainwin_info_text_locked = FALSE;
static guint mainwin_volume_release_timeout = 0;

static int ab_position_a = -1;
static int ab_position_b = -1;

static void change_timer_mode(void);
static void mainwin_position_motion_cb(GtkWidget *widget, gint pos);
static void mainwin_position_release_cb(GtkWidget *widget, gint pos);
static void mainwin_set_volume_diff (gint diff);

static void
mainwin_set_shade(gboolean shaded)
{
    GtkAction *action = gtk_action_group_get_action(toggleaction_group_others,
                                                    "roll up player");
    gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(action) , shaded );
}

void mainwin_set_shape (void)
{
    if (config.show_wm_decorations)
        gtk_widget_shape_combine_mask (mainwin, 0, 0, 0);
    else
        gtk_widget_shape_combine_mask (mainwin, skin_get_mask (aud_active_skin,
         config.player_shaded ? SKIN_MASK_MAIN_SHADE : SKIN_MASK_MAIN), 0, 0);
}

static void mainwin_vis_set_type (VisType mode)
{
    GtkAction *action;

    switch ( mode )
    {
        case VIS_ANALYZER:
            action = gtk_action_group_get_action(radioaction_group_vismode,
                                                 "vismode analyzer");
            break;
        case VIS_SCOPE:
            action = gtk_action_group_get_action(radioaction_group_vismode,
                                                 "vismode scope");
            break;
        case VIS_VOICEPRINT:
            action = gtk_action_group_get_action(radioaction_group_vismode,
                                                 "vismode voiceprint");
            break;
        case VIS_OFF:
        default:
            action = gtk_action_group_get_action(radioaction_group_vismode,
                                                 "vismode off");
            break;
    }

    gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(action) , TRUE );
}

static void
mainwin_menubtn_cb(void)
{
    gint x, y;
    gtk_window_get_position(GTK_WINDOW(mainwin), &x, &y);
    ui_popup_menu_show(UI_MENU_MAIN, x + 6 * MAINWIN_SCALE_FACTOR, y +
     MAINWIN_SHADED_HEIGHT * MAINWIN_SCALE_FACTOR, FALSE, FALSE, 1,
     GDK_CURRENT_TIME);
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
    mainwin_set_shade(!config.player_shaded);
}

static gboolean mainwin_vis_cb (GtkWidget * widget, GdkEventButton * event)
{
    if (event->button == 1) {
        config.vis_type++;

        if (config.vis_type > VIS_OFF)
            config.vis_type = VIS_ANALYZER;

        ui_vis_clear_data (mainwin_vis);
        ui_svis_clear_data (mainwin_svis);

        mainwin_vis_set_type(config.vis_type);
    }
    else if (event->button == 3)
        ui_popup_menu_show(UI_MENU_VISUALIZATION, event->x_root, event->y_root,
         FALSE, FALSE, 3, event->time);

    return TRUE;
}

static void show_main_menu (GdkEventButton * event, void * unused)
{
    GdkScreen * screen = gdk_event_get_screen ((GdkEvent *) event);
    gint width = gdk_screen_get_width (screen);
    gint height = gdk_screen_get_height (screen);

    ui_popup_menu_show (UI_MENU_MAIN, event->x_root, event->y_root,
     event->x_root > width / 2, event->y_root > height / 2, event->button,
     event->time);
}

static gchar *mainwin_tb_old_text = NULL;

void
mainwin_lock_info_text(const gchar * text)
{
    if (mainwin_info_text_locked != TRUE)
        mainwin_tb_old_text = g_strdup
         (aud_active_skin->properties.mainwin_othertext_is_status ?
         textbox_get_text (mainwin_othertext) : textbox_get_text (mainwin_info));

    mainwin_info_text_locked = TRUE;
    if (aud_active_skin->properties.mainwin_othertext_is_status)
        textbox_set_text (mainwin_othertext, text);
    else
        textbox_set_text (mainwin_info, text);
}

void
mainwin_release_info_text(void)
{
    mainwin_info_text_locked = FALSE;

    if (mainwin_tb_old_text != NULL)
    {
        if (aud_active_skin->properties.mainwin_othertext_is_status)
            textbox_set_text (mainwin_othertext, mainwin_tb_old_text);
        else
            textbox_set_text (mainwin_info, mainwin_tb_old_text);
        g_free(mainwin_tb_old_text);
        mainwin_tb_old_text = NULL;
    }
}

static gboolean status_message_enabled;

void mainwin_enable_status_message (gboolean enable)
{
    status_message_enabled = enable;
}

static gint status_message_source = 0;

static gboolean clear_status_message (void * unused)
{
    mainwin_release_info_text ();
    status_message_source = 0;
    return FALSE;
}

static void show_status_message (const gchar * message)
{
    if (! status_message_enabled)
        return;

    if (status_message_source)
        g_source_remove (status_message_source);

    mainwin_lock_info_text (message);
    status_message_source = g_timeout_add (1000, clear_status_message, NULL);
}

static gchar *
make_mainwin_title(const gchar * title)
{
    if (title != NULL)
        return g_strdup_printf(_("%s - Audacious"), title);
    else
        return g_strdup(_("Audacious"));
}

void
mainwin_set_song_title(const gchar * title)
{
    gchar *mainwin_title_text = make_mainwin_title(title);
    gtk_window_set_title(GTK_WINDOW(mainwin), mainwin_title_text);
    g_free(mainwin_title_text);

    mainwin_release_info_text ();
    textbox_set_text (mainwin_info, title != NULL ? title : "");
}

static void show_hide_widget (GtkWidget * widget, char show)
{
    if (show)
        gtk_widget_show (widget);
    else
        gtk_widget_hide (widget);
}

static void
mainwin_refresh_visible(void)
{
    show_hide_widget (mainwin_info,
     aud_active_skin->properties.mainwin_text_visible);
    show_hide_widget (mainwin_vis,
     aud_active_skin->properties.mainwin_vis_visible);
    show_hide_widget (mainwin_menurow,
     aud_active_skin->properties.mainwin_menurow_visible);
    show_hide_widget (mainwin_rate_text,
     ! aud_active_skin->properties.mainwin_othertext);
    show_hide_widget (mainwin_freq_text,
     ! aud_active_skin->properties.mainwin_othertext);
    show_hide_widget (mainwin_monostereo,
     ! aud_active_skin->properties.mainwin_othertext);
    show_hide_widget (mainwin_othertext,
     aud_active_skin->properties.mainwin_othertext &&
     aud_active_skin->properties.mainwin_othertext_visible);
}

void
mainwin_refresh_hints(void)
{
    /* positioning and size attributes */
    if (aud_active_skin->properties.mainwin_vis_x && aud_active_skin->properties.mainwin_vis_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_vis), aud_active_skin->properties.mainwin_vis_x,
                       aud_active_skin->properties.mainwin_vis_y);

    if (aud_active_skin->properties.mainwin_text_x && aud_active_skin->properties.mainwin_text_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_info), aud_active_skin->properties.mainwin_text_x,
                       aud_active_skin->properties.mainwin_text_y);

    if (aud_active_skin->properties.mainwin_text_width)
        textbox_set_width (mainwin_info,
         aud_active_skin->properties.mainwin_text_width);

    if (aud_active_skin->properties.mainwin_infobar_x && aud_active_skin->properties.mainwin_infobar_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_othertext), aud_active_skin->properties.mainwin_infobar_x,
                       aud_active_skin->properties.mainwin_infobar_y);

    if (aud_active_skin->properties.mainwin_number_0_x && aud_active_skin->properties.mainwin_number_0_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_minus_num), aud_active_skin->properties.mainwin_number_0_x,
                       aud_active_skin->properties.mainwin_number_0_y);

    if (aud_active_skin->properties.mainwin_number_1_x && aud_active_skin->properties.mainwin_number_1_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_10min_num), aud_active_skin->properties.mainwin_number_1_x,
                       aud_active_skin->properties.mainwin_number_1_y);

    if (aud_active_skin->properties.mainwin_number_2_x && aud_active_skin->properties.mainwin_number_2_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_min_num), aud_active_skin->properties.mainwin_number_2_x,
                       aud_active_skin->properties.mainwin_number_2_y);

    if (aud_active_skin->properties.mainwin_number_3_x && aud_active_skin->properties.mainwin_number_3_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_10sec_num), aud_active_skin->properties.mainwin_number_3_x,
                       aud_active_skin->properties.mainwin_number_3_y);

    if (aud_active_skin->properties.mainwin_number_4_x && aud_active_skin->properties.mainwin_number_4_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_sec_num), aud_active_skin->properties.mainwin_number_4_x,
                       aud_active_skin->properties.mainwin_number_4_y);

    if (aud_active_skin->properties.mainwin_playstatus_x && aud_active_skin->properties.mainwin_playstatus_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), mainwin_playstatus, aud_active_skin->properties.mainwin_playstatus_x,
                       aud_active_skin->properties.mainwin_playstatus_y);

    if (aud_active_skin->properties.mainwin_volume_x && aud_active_skin->properties.mainwin_volume_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_volume), aud_active_skin->properties.mainwin_volume_x,
                       aud_active_skin->properties.mainwin_volume_y);

    if (aud_active_skin->properties.mainwin_balance_x && aud_active_skin->properties.mainwin_balance_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_balance), aud_active_skin->properties.mainwin_balance_x,
                       aud_active_skin->properties.mainwin_balance_y);

    if (aud_active_skin->properties.mainwin_position_x && aud_active_skin->properties.mainwin_position_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_position), aud_active_skin->properties.mainwin_position_x,
                       aud_active_skin->properties.mainwin_position_y);

    if (aud_active_skin->properties.mainwin_previous_x && aud_active_skin->properties.mainwin_previous_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), mainwin_rew, aud_active_skin->properties.mainwin_previous_x,
                       aud_active_skin->properties.mainwin_previous_y);

    if (aud_active_skin->properties.mainwin_play_x && aud_active_skin->properties.mainwin_play_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_play), aud_active_skin->properties.mainwin_play_x,
                       aud_active_skin->properties.mainwin_play_y);

    if (aud_active_skin->properties.mainwin_pause_x && aud_active_skin->properties.mainwin_pause_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_pause), aud_active_skin->properties.mainwin_pause_x,
                       aud_active_skin->properties.mainwin_pause_y);

    if (aud_active_skin->properties.mainwin_stop_x && aud_active_skin->properties.mainwin_stop_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_stop), aud_active_skin->properties.mainwin_stop_x,
                       aud_active_skin->properties.mainwin_stop_y);

    if (aud_active_skin->properties.mainwin_next_x && aud_active_skin->properties.mainwin_next_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_fwd), aud_active_skin->properties.mainwin_next_x,
                       aud_active_skin->properties.mainwin_next_y);

    if (aud_active_skin->properties.mainwin_eject_x && aud_active_skin->properties.mainwin_eject_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_eject), aud_active_skin->properties.mainwin_eject_x,
                       aud_active_skin->properties.mainwin_eject_y);

    if (aud_active_skin->properties.mainwin_eqbutton_x && aud_active_skin->properties.mainwin_eqbutton_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_eq), aud_active_skin->properties.mainwin_eqbutton_x,
                       aud_active_skin->properties.mainwin_eqbutton_y);

    if (aud_active_skin->properties.mainwin_plbutton_x && aud_active_skin->properties.mainwin_plbutton_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_pl), aud_active_skin->properties.mainwin_plbutton_x,
                       aud_active_skin->properties.mainwin_plbutton_y);

    if (aud_active_skin->properties.mainwin_shuffle_x && aud_active_skin->properties.mainwin_shuffle_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_shuffle), aud_active_skin->properties.mainwin_shuffle_x,
                       aud_active_skin->properties.mainwin_shuffle_y);

    if (aud_active_skin->properties.mainwin_repeat_x && aud_active_skin->properties.mainwin_repeat_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_repeat), aud_active_skin->properties.mainwin_repeat_x,
                       aud_active_skin->properties.mainwin_repeat_y);

    if (aud_active_skin->properties.mainwin_about_x && aud_active_skin->properties.mainwin_about_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_about), aud_active_skin->properties.mainwin_about_x,
                       aud_active_skin->properties.mainwin_about_y);

    if (aud_active_skin->properties.mainwin_minimize_x && aud_active_skin->properties.mainwin_minimize_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_minimize), config.player_shaded ? 244 : aud_active_skin->properties.mainwin_minimize_x,
                       config.player_shaded ? 3 : aud_active_skin->properties.mainwin_minimize_y);

    if (aud_active_skin->properties.mainwin_shade_x && aud_active_skin->properties.mainwin_shade_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_shade), aud_active_skin->properties.mainwin_shade_x,
                       aud_active_skin->properties.mainwin_shade_y);

    if (aud_active_skin->properties.mainwin_close_x && aud_active_skin->properties.mainwin_close_y)
        gtk_fixed_move(GTK_FIXED(SKINNED_WINDOW(mainwin)->normal), GTK_WIDGET(mainwin_close), aud_active_skin->properties.mainwin_close_x,
                       aud_active_skin->properties.mainwin_close_y);

    mainwin_refresh_visible();

    if (config.player_shaded)
        resize_window(mainwin, MAINWIN_SHADED_WIDTH * MAINWIN_SCALE_FACTOR,
         MAINWIN_SHADED_HEIGHT * MAINWIN_SCALE_FACTOR);
    else if (aud_active_skin->properties.mainwin_height > 0 &&
     aud_active_skin->properties.mainwin_width > 0)
        resize_window(mainwin, aud_active_skin->properties.mainwin_width *
         MAINWIN_SCALE_FACTOR, aud_active_skin->properties.mainwin_height *
         MAINWIN_SCALE_FACTOR);
    else
        resize_window(mainwin, MAINWIN_WIDTH * MAINWIN_SCALE_FACTOR,
         MAINWIN_HEIGHT * MAINWIN_SCALE_FACTOR);
}

void mainwin_set_song_info (gint bitrate, gint samplerate, gint channels)
{
    gchar scratch[32];
    gint length;

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
        snprintf (scratch, sizeof scratch, "%d %s", bitrate / 1000, _("kbps"));
    else
        scratch[0] = 0;

    if (samplerate > 0)
    {
        length = strlen (scratch);
        snprintf (scratch + length, sizeof scratch - length, "%s%d %s", length ?
         ", " : "", samplerate / 1000, _("kHz"));
    }

    if (channels > 0)
    {
        length = strlen (scratch);
        snprintf (scratch + length, sizeof scratch - length, "%s%s", length ?
         ", " : "", channels > 2 ? _("surround") : channels > 1 ? _("stereo") :
         _("mono"));
    }

    textbox_set_text (mainwin_othertext, scratch);
}

void
mainwin_clear_song_info(void)
{
    mainwin_set_song_title (NULL);

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

    UI_SKINNED_HORIZONTAL_SLIDER(mainwin_position)->pressed = FALSE;
    UI_SKINNED_HORIZONTAL_SLIDER(mainwin_sposition)->pressed = FALSE;

    /* clear sampling parameter displays */
    textbox_set_text (mainwin_rate_text, "   ");
    textbox_set_text (mainwin_freq_text, "  ");
    ui_skinned_monostereo_set_num_channels(mainwin_monostereo, 0);
    textbox_set_text (mainwin_othertext, "");

    if (mainwin_playstatus != NULL)
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
    }
}

static gboolean
mainwin_widget_contained(GdkEventButton *event, int x, int y, int w, int h)
{
    gint ex = event->x / MAINWIN_SCALE_FACTOR;
    gint ey = event->y / MAINWIN_SCALE_FACTOR;

    return (ex > x && ey > y && ex < x + w && ey < y + h);
}

static gboolean
mainwin_mouse_button_press(GtkWidget * widget,
                           GdkEventButton * event,
                           gpointer callback_data)
{
    if (event->button == 1 && event->type == GDK_2BUTTON_PRESS && event->y /
     MAINWIN_SCALE_FACTOR < 14)
    {
        mainwin_set_shade(!config.player_shaded);
        if (dock_is_moving(GTK_WINDOW(mainwin)))
            dock_move_release(GTK_WINDOW(mainwin));
        return TRUE;
    }

    if (event->button == 3) {
        /* Pop up playback menu if right clicked over playback-control widgets,
         * otherwise popup general menu
         */
        if (mainwin_widget_contained(event, aud_active_skin->properties.mainwin_position_x,
                                     aud_active_skin->properties.mainwin_position_y, 248, 10) ||
            mainwin_widget_contained(event, aud_active_skin->properties.mainwin_previous_x,
                                     aud_active_skin->properties.mainwin_previous_y, 23, 18) ||
            mainwin_widget_contained(event, aud_active_skin->properties.mainwin_play_x,
                                     aud_active_skin->properties.mainwin_play_y, 23, 18) ||
            mainwin_widget_contained(event, aud_active_skin->properties.mainwin_pause_x,
                                     aud_active_skin->properties.mainwin_pause_y, 23, 18) ||
            mainwin_widget_contained(event, aud_active_skin->properties.mainwin_stop_x,
                                     aud_active_skin->properties.mainwin_stop_y, 23, 18) ||
            mainwin_widget_contained(event, aud_active_skin->properties.mainwin_next_x,
                                     aud_active_skin->properties.mainwin_next_y, 23, 18))
            ui_popup_menu_show(UI_MENU_PLAYBACK, event->x_root, event->y_root,
             FALSE, FALSE, 3, event->time);
        else
            ui_popup_menu_show(UI_MENU_MAIN, event->x_root, event->y_root,
             FALSE, FALSE, 3, event->time);

        return TRUE;
    }

    return FALSE;
}

gboolean mainwin_keypress (GtkWidget * widget, GdkEventKey * event,
 void * unused)
{
    if (ui_skinned_playlist_key (playlistwin_list, event))
        return 1;

    switch (event->keyval)
    {
        case GDK_minus:
            mainwin_set_volume_diff (-5);
            break;
        case GDK_plus:
            mainwin_set_volume_diff (5);
            break;
        case GDK_Left:
        case GDK_KP_Left:
        case GDK_KP_7:
            aud_drct_seek (aud_drct_get_time () - 5000);
            break;
        case GDK_Right:
        case GDK_KP_Right:
        case GDK_KP_9:
            aud_drct_seek (aud_drct_get_time () + 5000);
            break;
        case GDK_KP_4:
            aud_drct_pl_prev ();
            break;
        case GDK_KP_6:
            aud_drct_pl_next ();
            break;
        case GDK_KP_Insert:
            audgui_jump_to_track ();
            break;
        case GDK_space:
            aud_drct_pause();
            break;
        case GDK_Tab: /* GtkUIManager does not handle tab, apparently. */
            if (event->state & GDK_SHIFT_MASK)
                action_playlist_prev ();
            else
                action_playlist_next ();

            break;
        case GDK_ISO_Left_Tab:
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
                           gint x,
                           gint y,
                           GtkSelectionData * selection_data,
                           guint info,
                           guint time,
                           gpointer user_data)
{
    g_return_if_fail(selection_data != NULL);
    g_return_if_fail(selection_data->data != NULL);

    if (str_has_prefix_nocase((gchar *) selection_data->data, "fonts:///"))
    {
        gchar *path = (gchar *) selection_data->data;
        gchar *decoded = g_filename_from_uri(path, NULL, NULL);

        if (decoded == NULL)
            return;

        config.playlist_font = g_strconcat(decoded, strrchr(config.playlist_font, ' '), NULL);
        ui_skinned_playlist_set_font (playlistwin_list, config.playlist_font);

        g_free(decoded);

        return;
    }

    /* perhaps make suffix check case-insensitive -- desowin */
    if (str_has_prefix_nocase((char*)selection_data->data, "file:///")) {
        if (str_has_suffix_nocase((char*)selection_data->data, ".wsz\r\n") ||
            str_has_suffix_nocase((char*)selection_data->data, ".zip\r\n")) {
            on_skin_view_drag_data_received(GTK_WIDGET(user_data), context, x, y, selection_data, info, time, NULL);
            return;
        }
    }

    audgui_urilist_open ((const gchar *) selection_data->data);
}

static gint time_now (void)
{
    struct timeval tv;
    gettimeofday (& tv, NULL);
    return (tv.tv_sec % (24 * 3600) * 1000 + tv.tv_usec / 1000);
}

static gint time_diff (gint a, gint b)
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

    gint held = time_diff (seek_time, time_now ());
    if (held < SEEK_THRESHOLD)
        return TRUE;

    gint position;
    if (GPOINTER_TO_INT (rewind))
        position = seek_start - held / SEEK_SPEED;
    else
        position = seek_start + held / SEEK_SPEED;

    position = CLAMP (position, 0, 219);
    ui_skinned_horizontal_slider_set_position (mainwin_position, position);
    mainwin_position_motion_cb (mainwin_position, position);

    return TRUE;
}

static gboolean seek_press (GtkWidget * widget, GdkEventButton * event,
 gboolean rewind)
{
    if (event->button != 1 || seek_source)
        return FALSE;

    seek_start = ui_skinned_horizontal_slider_get_position (mainwin_position);
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
        mainwin_position_release_cb (mainwin_position,
         ui_skinned_horizontal_slider_get_position (mainwin_position));

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

void
mainwin_play_pushed(void)
{
    if (ab_position_a != -1)
        aud_drct_seek(ab_position_a / 1000);

    aud_drct_play ();
}

static void mainwin_shuffle_cb (GtkWidget * button, GdkEventButton * event)
 {check_set (toggleaction_group_others, "playback shuffle", button_get_active (button)); }
static void mainwin_repeat_cb (GtkWidget * button, GdkEventButton * event)
 {check_set (toggleaction_group_others, "playback repeat", button_get_active (button)); }
static void mainwin_eq_cb (GtkWidget * button, GdkEventButton * event)
 {equalizerwin_show (button_get_active (button)); }
static void mainwin_pl_cb (GtkWidget * button, GdkEventButton * event)
 {playlistwin_show (button_get_active (button)); }

static gint mainwin_spos_frame_cb (gint pos)
{
    if (mainwin_sposition) {
        gint x = 0;
        if (pos < 6)
            x = 17;
        else if (pos < 9)
            x = 20;
        else
            x = 23;

        UI_SKINNED_HORIZONTAL_SLIDER(mainwin_sposition)->knob_nx = x;
        UI_SKINNED_HORIZONTAL_SLIDER(mainwin_sposition)->knob_px = x;
    }
    return 1;
}

static void mainwin_spos_motion_cb (GtkWidget * widget, gint pos)
{
    gint time;
    gchar *time_msg;

    pos--;

    time = aud_drct_get_length () / 1000 * pos / 12;

    if (config.timer_mode == TIMER_REMAINING) {
        time = aud_drct_get_length () / 1000 - time;
        time_msg = g_strdup_printf("-%2.2d", time / 60);
        textbox_set_text (mainwin_stime_min, time_msg);
        g_free(time_msg);
    }
    else {
        time_msg = g_strdup_printf(" %2.2d", time / 60);
        textbox_set_text (mainwin_stime_min, time_msg);
        g_free(time_msg);
    }

    time_msg = g_strdup_printf("%2.2d", time % 60);
    textbox_set_text (mainwin_stime_sec, time_msg);
    g_free(time_msg);
}

static void mainwin_spos_release_cb (GtkWidget * widget, gint pos)
{
    aud_drct_seek (aud_drct_get_length () * (pos - 1) / 12);
}

static void mainwin_position_motion_cb (GtkWidget * widget, gint pos)
{
    gint length, time;
    gchar *seek_msg;

    length = aud_drct_get_length () / 1000;
    time = (length * pos) / 219;
    seek_msg = g_strdup_printf(_("Seek to: %d:%-2.2d/%d:%-2.2d (%d%%)"),
                               time / 60, time % 60,
                               length / 60, length % 60,
                               (length != 0) ? (time * 100) / length : 0);
    mainwin_lock_info_text(seek_msg);
    g_free(seek_msg);
}

static void mainwin_position_release_cb (GtkWidget * widget, gint pos)
{
    gint length, time;

    length = aud_drct_get_length();
    time = (gint64) length * pos / 219;
    aud_drct_seek(time);
    mainwin_release_info_text();
}

static gint mainwin_volume_frame_cb (gint pos)
{
    return (gint) rint((pos / 52.0) * 28);
}

void
mainwin_adjust_volume_motion(gint v)
{
    gchar *volume_msg;

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
mainwin_adjust_balance_motion(gint b)
{
    gchar *balance_msg;

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

void
mainwin_set_volume_slider(gint percent)
{
    ui_skinned_horizontal_slider_set_position(mainwin_volume, (gint) rint((percent * 51) / 100.0));
}

void
mainwin_set_balance_slider(gint percent)
{
    ui_skinned_horizontal_slider_set_position(mainwin_balance, (gint) rint(((percent * 12) / 100.0) + 12));
}

static void mainwin_volume_motion_cb (GtkWidget * widget, gint pos)
{
    gint vol = (pos * 100) / 51;
    mainwin_adjust_volume_motion(vol);
    equalizerwin_set_volume_slider(vol);
}

static gboolean mainwin_volume_release_cb (GtkWidget * widget, gint pos)
{
    mainwin_adjust_volume_release();
    return FALSE;
}

static gint mainwin_balance_frame_cb (gint pos)
{
    return ((abs(pos - 12) * 28) / 13);
}

static void mainwin_balance_motion_cb (GtkWidget * widget, gint pos)
{
    gint bal = ((pos - 12) * 100) / 12;
    mainwin_adjust_balance_motion(bal);
    equalizerwin_set_balance_slider(bal);
}

static void mainwin_balance_release_cb (GtkWidget * widget, gint pos)
{
    mainwin_adjust_volume_release();
}

static void mainwin_set_volume_diff (gint diff)
{
    gint vol;

    aud_drct_get_volume_main (& vol);
    vol = CLAMP (vol + diff, 0, 100);
    mainwin_adjust_volume_motion(vol);
    mainwin_set_volume_slider(vol);
    equalizerwin_set_volume_slider(vol);

    if (mainwin_volume_release_timeout)
        g_source_remove(mainwin_volume_release_timeout);
    mainwin_volume_release_timeout =
        g_timeout_add(700, (GSourceFunc)(mainwin_volume_release_cb), NULL);
}

static void mainwin_real_show (gboolean show)
{
    start_stop_visual (FALSE);

    if (show)
        gtk_window_present ((GtkWindow *) mainwin);
    else
        gtk_widget_hide (mainwin);
}

void mainwin_show (gboolean show)
{
    GtkAction * a;

    a = gtk_action_group_get_action (toggleaction_group_others, "show player");

    if (a && gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (a)) != show)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (a), show);
    else
    {
        config.player_visible = show;
        playlistwin_show (config.playlist_visible);
        equalizerwin_show (config.equalizer_visible);
        mainwin_real_show (show);
   }
}

void mainwin_mr_change (MenuRowItem i)
{
    switch (i) {
        case MENUROW_OPTIONS:
            mainwin_lock_info_text(_("Options Menu"));
            break;
        case MENUROW_ALWAYS:
            if (config.always_on_top)
                mainwin_lock_info_text(_("Disable 'Always On Top'"));
            else
                mainwin_lock_info_text(_("Enable 'Always On Top'"));
            break;
        case MENUROW_FILEINFOBOX:
            mainwin_lock_info_text(_("File Info Box"));
            break;
        case MENUROW_SCALE:
            if (config.scaled)
                mainwin_lock_info_text(_("Disable 'GUI Scaling'"));
            else
                mainwin_lock_info_text(_("Enable 'GUI Scaling'"));
            break;
        case MENUROW_VISUALIZATION:
            mainwin_lock_info_text(_("Visualization Menu"));
            break;
        case MENUROW_NONE:
            break;
    }
}

void mainwin_mr_release (MenuRowItem i, GdkEventButton * event)
{
    switch (i) {
        case MENUROW_OPTIONS:
            ui_popup_menu_show(UI_MENU_VIEW, event->x_root, event->y_root,
             FALSE, FALSE, 1, event->time);
            break;
        case MENUROW_ALWAYS:
            gtk_toggle_action_set_active ((GtkToggleAction *)
             gtk_action_group_get_action (toggleaction_group_others,
             "view always on top"), config.always_on_top);
            break;
        case MENUROW_FILEINFOBOX:
            audgui_infowin_show_current ();
            break;
        case MENUROW_SCALE:
            break;
        case MENUROW_VISUALIZATION:
            ui_popup_menu_show(UI_MENU_VISUALIZATION, event->x_root,
             event->y_root, FALSE, FALSE, 1, event->time);
            break;
        case MENUROW_NONE:
            break;
    }

    mainwin_release_info_text();
}

static void
set_timer_mode(TimerMode mode)
{
    if (mode == TIMER_ELAPSED)
        check_set(radioaction_group_viewtime, "view time elapsed", TRUE);
    else
        check_set(radioaction_group_viewtime, "view time remaining", TRUE);
}

gboolean
change_timer_mode_cb(GtkWidget *widget, GdkEventButton *event)
{
    if (event->button == 1) {
        change_timer_mode();
    } else if (event->button == 3)
        return FALSE;

    return TRUE;
}

static void change_timer_mode(void) {
    if (config.timer_mode == TIMER_ELAPSED)
        set_timer_mode(TIMER_REMAINING);
    else
        set_timer_mode(TIMER_ELAPSED);
    if (aud_drct_get_playing())
        mainwin_update_song_info();
}

void
mainwin_setup_menus(void)
{
    set_timer_mode(config.timer_mode);

    /* View menu */

    check_set(toggleaction_group_others, "view always on top", config.always_on_top);
    check_set(toggleaction_group_others, "view put on all workspaces", config.sticky);
    check_set(toggleaction_group_others, "roll up player", config.player_shaded);
    check_set(toggleaction_group_others, "roll up playlist editor", config.playlist_shaded);
    check_set(toggleaction_group_others, "roll up equalizer", config.equalizer_shaded);
    check_set(toggleaction_group_others, "view easy move", config.easy_move);

    mainwin_enable_status_message (FALSE);

    /* Songname menu */

    check_set(toggleaction_group_others, "autoscroll songname", config.autoscroll);
    check_set(toggleaction_group_others, "stop after current song", aud_cfg->stopaftersong);

    /* Playback menu */

    check_set(toggleaction_group_others, "playback repeat", aud_cfg->repeat);
    check_set(toggleaction_group_others, "playback shuffle", aud_cfg->shuffle);
    check_set(toggleaction_group_others, "playback no playlist advance", aud_cfg->no_playlist_advance);

    mainwin_enable_status_message (TRUE);

    /* Visualization menu */

    switch ( config.vis_type )
    {
        case VIS_ANALYZER:
            check_set(radioaction_group_vismode, "vismode analyzer", TRUE);
            break;
        case VIS_SCOPE:
            check_set(radioaction_group_vismode, "vismode scope", TRUE);
            break;
        case VIS_VOICEPRINT:
            check_set(radioaction_group_vismode, "vismode voiceprint", TRUE);
            break;
        case VIS_OFF:
        default:
            check_set(radioaction_group_vismode, "vismode off", TRUE);
            break;
    }

    switch ( config.analyzer_mode )
    {
        case ANALYZER_FIRE:
            check_set(radioaction_group_anamode, "anamode fire", TRUE);
            break;
        case ANALYZER_VLINES:
            check_set(radioaction_group_anamode, "anamode vertical lines", TRUE);
            break;
        case ANALYZER_NORMAL:
        default:
            check_set(radioaction_group_anamode, "anamode normal", TRUE);
            break;
    }

    switch ( config.analyzer_type )
    {
        case ANALYZER_BARS:
            check_set(radioaction_group_anatype, "anatype bars", TRUE);
            break;
        case ANALYZER_LINES:
        default:
            check_set(radioaction_group_anatype, "anatype lines", TRUE);
            break;
    }

    check_set(toggleaction_group_others, "anamode peaks", config.analyzer_peaks );

    switch ( config.scope_mode )
    {
        case SCOPE_LINE:
            check_set(radioaction_group_scomode, "scomode line", TRUE);
            break;
        case SCOPE_SOLID:
            check_set(radioaction_group_scomode, "scomode solid", TRUE);
            break;
        case SCOPE_DOT:
        default:
            check_set(radioaction_group_scomode, "scomode dot", TRUE);
            break;
    }

    switch ( config.voiceprint_mode )
    {
        case VOICEPRINT_FIRE:
            check_set(radioaction_group_vprmode, "vprmode fire", TRUE);
            break;
        case VOICEPRINT_ICE:
            check_set(radioaction_group_vprmode, "vprmode ice", TRUE);
            break;
        case VOICEPRINT_NORMAL:
        default:
            check_set(radioaction_group_vprmode, "vprmode normal", TRUE);
            break;
    }

    switch ( config.vu_mode )
    {
        case VU_SMOOTH:
            check_set(radioaction_group_wshmode, "wshmode smooth", TRUE);
            break;
        case VU_NORMAL:
        default:
            check_set(radioaction_group_wshmode, "wshmode normal", TRUE);
            break;
    }

    switch ( config.analyzer_falloff )
    {
        case FALLOFF_SLOW:
            check_set(radioaction_group_anafoff, "anafoff slow", TRUE);
            break;
        case FALLOFF_MEDIUM:
            check_set(radioaction_group_anafoff, "anafoff medium", TRUE);
            break;
        case FALLOFF_FAST:
            check_set(radioaction_group_anafoff, "anafoff fast", TRUE);
            break;
        case FALLOFF_FASTEST:
            check_set(radioaction_group_anafoff, "anafoff fastest", TRUE);
            break;
        case FALLOFF_SLOWEST:
        default:
            check_set(radioaction_group_anafoff, "anafoff slowest", TRUE);
            break;
    }

    switch ( config.peaks_falloff )
    {
        case FALLOFF_SLOW:
            check_set(radioaction_group_peafoff, "peafoff slow", TRUE);
            break;
        case FALLOFF_MEDIUM:
            check_set(radioaction_group_peafoff, "peafoff medium", TRUE);
            break;
        case FALLOFF_FAST:
            check_set(radioaction_group_peafoff, "peafoff fast", TRUE);
            break;
        case FALLOFF_FASTEST:
            check_set(radioaction_group_peafoff, "peafoff fastest", TRUE);
            break;
        case FALLOFF_SLOWEST:
        default:
            check_set(radioaction_group_peafoff, "peafoff slowest", TRUE);
            break;
    }
}

static gboolean mainwin_info_button_press (GtkWidget * widget, GdkEventButton *
 event)
{
    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        ui_popup_menu_show (UI_MENU_SONGNAME, event->x_root, event->y_root,
         FALSE, FALSE, event->button, event->time);
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
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_menubtn, 6, 3);
    button_on_release (mainwin_menubtn, (ButtonCB) mainwin_menubtn_cb);

    mainwin_minimize = button_new (9, 9, 9, 0, 9, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_minimize, 244, 3);
    button_on_release (mainwin_minimize, (ButtonCB) mainwin_minimize_cb);

    mainwin_shade = button_new (9, 9, 0, 18, 9, 18, SKIN_TITLEBAR, SKIN_TITLEBAR);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_shade, 254, 3);
    button_on_release (mainwin_shade, (ButtonCB) mainwin_shade_toggle);

    mainwin_close = button_new (9, 9, 18, 0, 18, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_close, 264, 3);
    button_on_release (mainwin_close, (ButtonCB) aud_drct_quit);

    mainwin_rew = button_new (23, 18, 0, 0, 0, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_rew, 16, 88);
    button_on_press (mainwin_rew, mainwin_rew_press);
    button_on_release (mainwin_rew, mainwin_rew_release);

    mainwin_fwd = button_new (22, 18, 92, 0, 92, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_fwd, 108, 88);
    button_on_press (mainwin_fwd, mainwin_fwd_press);
    button_on_release (mainwin_fwd, mainwin_fwd_release);

    mainwin_play = button_new (23, 18, 23, 0, 23, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_play, 39, 88);
    button_on_release (mainwin_play, (ButtonCB) mainwin_play_pushed);

    mainwin_pause = button_new (23, 18, 46, 0, 46, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_pause, 62, 88);
    button_on_release (mainwin_pause, (ButtonCB) aud_drct_pause);

    mainwin_stop = button_new (23, 18, 69, 0, 69, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_stop, 85, 88);
    button_on_release (mainwin_stop, (ButtonCB) aud_drct_stop);

    mainwin_eject = button_new (22, 16, 114, 0, 114, 16, SKIN_CBUTTONS, SKIN_CBUTTONS);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_eject, 136, 89);
    button_on_release (mainwin_eject, (ButtonCB) action_play_file);

    mainwin_shuffle = button_new_toggle (46, 15, 28, 0, 28, 15, 28, 30, 28, 45, SKIN_SHUFREP, SKIN_SHUFREP);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_shuffle, 164, 89);
    button_on_release (mainwin_shuffle, mainwin_shuffle_cb);

    mainwin_repeat = button_new_toggle (28, 15, 0, 0, 0, 15, 0, 30, 0, 45, SKIN_SHUFREP, SKIN_SHUFREP);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_repeat, 210, 89);
    button_on_release (mainwin_repeat, mainwin_repeat_cb);

    mainwin_eq = button_new_toggle (23, 12, 0, 61, 46, 61, 0, 73, 46, 73, SKIN_SHUFREP, SKIN_SHUFREP);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_eq, 219, 58);
    button_on_release (mainwin_eq, mainwin_eq_cb);

    mainwin_pl = button_new_toggle (23, 12, 23, 61, 69, 61, 23, 73, 69, 73, SKIN_SHUFREP, SKIN_SHUFREP);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_pl, 242, 58);
    button_on_release (mainwin_pl, mainwin_pl_cb);

    mainwin_info = textbox_new (153);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_info, 112, 27);
    textbox_set_font (mainwin_info, config.mainwin_use_bitmapfont ? NULL : config.mainwin_font);
    textbox_set_scroll (mainwin_info, config.autoscroll);
    g_signal_connect (mainwin_info, "button-press-event", (GCallback)
     mainwin_info_button_press, NULL);

    mainwin_othertext = textbox_new (153);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_othertext, 112, 43);

    mainwin_rate_text = textbox_new (15);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_rate_text, 111, 43);

    mainwin_freq_text = textbox_new (10);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_freq_text, 156, 43);

    mainwin_menurow = ui_skinned_menurow_new ();
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_menurow, 10, 22);

    mainwin_volume = ui_skinned_horizontal_slider_new(SKINNED_WINDOW(mainwin)->normal, 107, 57, 68,
                                                      13, 15, 422, 0, 422, 14, 11, 15, 0, 0, 51,
                                                      mainwin_volume_frame_cb, SKIN_VOLUME);
    g_signal_connect(mainwin_volume, "motion", G_CALLBACK(mainwin_volume_motion_cb), NULL);
    g_signal_connect(mainwin_volume, "release", G_CALLBACK(mainwin_volume_release_cb), NULL);

    mainwin_balance = ui_skinned_horizontal_slider_new(SKINNED_WINDOW(mainwin)->normal, 177, 57, 38,
                                                       13, 15, 422, 0, 422, 14, 11, 15, 9, 0, 24,
                                                       mainwin_balance_frame_cb, SKIN_BALANCE);
    g_signal_connect(mainwin_balance, "motion", G_CALLBACK(mainwin_balance_motion_cb), NULL);
    g_signal_connect(mainwin_balance, "release", G_CALLBACK(mainwin_balance_release_cb), NULL);

    mainwin_monostereo = ui_skinned_monostereo_new(SKINNED_WINDOW(mainwin)->normal, 212, 41, SKIN_MONOSTEREO);

    mainwin_playstatus = ui_skinned_playstatus_new(SKINNED_WINDOW(mainwin)->normal, 24, 28);

    mainwin_minus_num = ui_skinned_number_new(SKINNED_WINDOW(mainwin)->normal, 36, 26, SKIN_NUMBERS);
    g_signal_connect(mainwin_minus_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);

    mainwin_10min_num = ui_skinned_number_new(SKINNED_WINDOW(mainwin)->normal, 48, 26, SKIN_NUMBERS);
    g_signal_connect(mainwin_10min_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);

    mainwin_min_num = ui_skinned_number_new(SKINNED_WINDOW(mainwin)->normal, 60, 26, SKIN_NUMBERS);
    g_signal_connect(mainwin_min_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);

    mainwin_10sec_num = ui_skinned_number_new(SKINNED_WINDOW(mainwin)->normal, 78, 26, SKIN_NUMBERS);
    g_signal_connect(mainwin_10sec_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);

    mainwin_sec_num = ui_skinned_number_new(SKINNED_WINDOW(mainwin)->normal, 90, 26, SKIN_NUMBERS);
    g_signal_connect(mainwin_sec_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);

    mainwin_about = button_new_small (20, 25);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal, mainwin_about, 247, 83);
    button_on_release (mainwin_about, (ButtonCB) audgui_show_about_window);

    mainwin_vis = ui_vis_new ();
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->normal,
     mainwin_vis, 24, 43);
    g_signal_connect(mainwin_vis, "button-press-event", G_CALLBACK(mainwin_vis_cb), NULL);

    mainwin_position = ui_skinned_horizontal_slider_new(SKINNED_WINDOW(mainwin)->normal, 16, 72, 248,
                                                        10, 248, 0, 278, 0, 29, 10, 10, 0, 0, 219,
                                                        NULL, SKIN_POSBAR);
    g_signal_connect(mainwin_position, "motion", G_CALLBACK(mainwin_position_motion_cb), NULL);
    g_signal_connect(mainwin_position, "release", G_CALLBACK(mainwin_position_release_cb), NULL);

    /* shaded */

    mainwin_shaded_menubtn = button_new (9, 9, 0, 0, 0, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded, mainwin_shaded_menubtn, 6, 3);
    button_on_release (mainwin_shaded_menubtn, (ButtonCB) mainwin_menubtn_cb);

    mainwin_shaded_minimize = button_new (9, 9, 9, 0, 9, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded, mainwin_shaded_minimize, 244, 3);
    button_on_release (mainwin_shaded_minimize, (ButtonCB) mainwin_minimize_cb);

    mainwin_shaded_shade = button_new (9, 9, 0, 27, 9, 27, SKIN_TITLEBAR, SKIN_TITLEBAR);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded, mainwin_shaded_shade, 254, 3);
    button_on_release (mainwin_shaded_shade, (ButtonCB) mainwin_shade_toggle);

    mainwin_shaded_close = button_new (9, 9, 18, 0, 18, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded, mainwin_shaded_close, 264, 3);
    button_on_release (mainwin_shaded_close, (ButtonCB) aud_drct_quit);

    mainwin_srew = button_new_small (8, 7);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded, mainwin_srew, 169, 4);
    button_on_release (mainwin_srew, (ButtonCB) aud_drct_pl_prev);

    mainwin_splay = button_new_small (10, 7);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded, mainwin_splay, 177, 4);
    button_on_release (mainwin_splay, (ButtonCB) mainwin_play_pushed);

    mainwin_spause = button_new_small (10, 7);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded, mainwin_spause, 187, 4);
    button_on_release (mainwin_spause, (ButtonCB) aud_drct_pause);

    mainwin_sstop = button_new_small (9, 7);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded, mainwin_sstop, 197, 4);
    button_on_release (mainwin_sstop, (ButtonCB) aud_drct_stop);

    mainwin_sfwd = button_new_small (8, 7);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded, mainwin_sfwd, 206, 4);
    button_on_release (mainwin_sfwd, (ButtonCB) aud_drct_pl_next);

    mainwin_seject = button_new_small (9, 7);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded, mainwin_seject, 216, 4);
    button_on_release (mainwin_seject, (ButtonCB) action_play_file);

    mainwin_svis = ui_svis_new ();
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded,
     mainwin_svis, 79, 5);
    g_signal_connect(mainwin_svis, "button-press-event", G_CALLBACK(mainwin_vis_cb), NULL);

    mainwin_sposition = ui_skinned_horizontal_slider_new(SKINNED_WINDOW(mainwin)->shaded, 226, 4, 17,
                                                         7, 17, 36, 17, 36, 3, 7, 36, 0, 1, 13,
                                                         mainwin_spos_frame_cb, SKIN_TITLEBAR);
    g_signal_connect(mainwin_sposition, "motion", G_CALLBACK(mainwin_spos_motion_cb), NULL);
    g_signal_connect(mainwin_sposition, "release", G_CALLBACK(mainwin_spos_release_cb), NULL);

    mainwin_stime_min = textbox_new (15);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded, mainwin_stime_min, 130, 4);

    mainwin_stime_sec = textbox_new (10);
    gtk_fixed_put ((GtkFixed *) ((SkinnedWindow *) mainwin)->shaded, mainwin_stime_sec, 147, 4);

    g_signal_connect(mainwin_stime_min, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);
    g_signal_connect(mainwin_stime_sec, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);
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

    gtk_widget_set_no_show_all (mainwin_info, TRUE);
    gtk_widget_set_no_show_all (mainwin_vis, TRUE);
    gtk_widget_set_no_show_all (mainwin_menurow, TRUE);
    gtk_widget_set_no_show_all (mainwin_rate_text, TRUE);
    gtk_widget_set_no_show_all (mainwin_freq_text, TRUE);
    gtk_widget_set_no_show_all (mainwin_monostereo, TRUE);
    gtk_widget_set_no_show_all (mainwin_othertext, TRUE);

    gtk_widget_show_all (((SkinnedWindow *) mainwin)->normal);
    gtk_widget_show_all (((SkinnedWindow *) mainwin)->shaded);

    ui_skinned_window_set_shade (mainwin, config.player_shaded);
}

static gboolean state_cb (GtkWidget * widget, GdkEventWindowState * event,
 void * unused)
{
    if (event->changed_mask & GDK_WINDOW_STATE_STICKY)
    {
        gboolean sticky = (event->new_window_state & GDK_WINDOW_STATE_STICKY);
        GtkToggleAction * action = (GtkToggleAction *)
         gtk_action_group_get_action (toggleaction_group_others,
         "view put on all workspaces");
        gtk_toggle_action_set_active (action, sticky);
    }

    if (event->changed_mask & GDK_WINDOW_STATE_ABOVE)
    {
        gboolean above = (event->new_window_state & GDK_WINDOW_STATE_ABOVE);
        GtkToggleAction * action = (GtkToggleAction *)
         gtk_action_group_get_action (toggleaction_group_others,
         "view always on top");
        gtk_toggle_action_set_active (action, above);
    }

    return TRUE;
}

static gboolean delete_cb (GtkWidget * widget, GdkEvent * event, void * unused)
{
    aud_drct_quit ();
    return TRUE;
}

static void
mainwin_create_window(void)
{
    gint width, height;

    mainwin = ui_skinned_window_new("player", &config.player_x, &config.player_y);
    gtk_window_set_title(GTK_WINDOW(mainwin), _("Audacious"));
    gtk_window_set_role(GTK_WINDOW(mainwin), "player");
    gtk_window_set_resizable(GTK_WINDOW(mainwin), FALSE);

    width = config.player_shaded ? MAINWIN_SHADED_WIDTH : aud_active_skin->properties.mainwin_width;
    height = config.player_shaded ? MAINWIN_SHADED_HEIGHT : aud_active_skin->properties.mainwin_height;

    if (config.scaled) {
        width *= config.scale_factor;
        height *= config.scale_factor;
    }

    gtk_widget_set_size_request(mainwin, width, height);

    g_signal_connect(mainwin, "button_press_event",
                     G_CALLBACK(mainwin_mouse_button_press), NULL);
    g_signal_connect(mainwin, "scroll_event",
                     G_CALLBACK(mainwin_scrolled), NULL);

    aud_drag_dest_set(mainwin);
    g_signal_connect ((GObject *) mainwin, "drag-data-received", (GCallback)
     mainwin_drag_data_received, 0);

    g_signal_connect(mainwin, "key_press_event",
                     G_CALLBACK(mainwin_keypress), NULL);

    ui_main_evlistener_init();

    g_signal_connect ((GObject *) mainwin, "window-state-event", (GCallback)
     state_cb, NULL);
    g_signal_connect ((GObject *) mainwin, "delete-event", (GCallback)
     delete_cb, NULL);
}

void mainwin_unhook (void)
{
    if (seek_source != 0)
    {
        g_source_remove (seek_source);
        seek_source = 0;
    }

    hook_dissociate ("show main menu", (HookFunction) show_main_menu);
    ui_main_evlistener_dissociate ();
    start_stop_visual (TRUE);
}

void
mainwin_create(void)
{
    mainwin_create_window();

    gtk_window_add_accel_group( GTK_WINDOW(mainwin) , ui_manager_get_accel_group() );

    mainwin_create_widgets();
    show_widgets ();

    hook_associate ("show main menu", (HookFunction) show_main_menu, 0);
    status_message_enabled = TRUE;
}

static void mainwin_update_volume (void)
{
    gint volume, balance;

    aud_drct_get_volume_main (& volume);
    aud_drct_get_volume_balance (& balance);
    mainwin_set_volume_slider (volume);
    mainwin_set_balance_slider (balance);
}

static void mainwin_update_time_display (gint time, gint length)
{
    gchar scratch[7];

    if (config.timer_mode == TIMER_REMAINING && length > 0)
    {
        if (length - time < 60000)         /* " -0:SS" */
            snprintf (scratch, sizeof scratch, " -0:%02d", (length - time) /
             1000);
        else if (length - time < 6000000)  /* "-MM:SS" */
            snprintf (scratch, sizeof scratch, "%3d:%02d", (time - length) /
             60000, (length - time) / 1000 % 60);
        else                               /* "-HH:MM" */
            snprintf (scratch, sizeof scratch, "%3d:%02d", (time - length) /
             3600000, (length - time) / 60000 % 60);
    }
    else
    {
        if (time < 60000000)  /* MMM:SS */
            snprintf (scratch, sizeof scratch, "%3d:%02d", time / 60000, time /
             1000 % 60);
        else                  /* HHH:MM */
            snprintf (scratch, sizeof scratch, "%3d:%02d", time / 3600000, time
             / 60000 % 60);
    }

    scratch[3] = 0;

    ui_skinned_number_set (mainwin_minus_num, scratch[0]);
    ui_skinned_number_set (mainwin_10min_num, scratch[1]);
    ui_skinned_number_set (mainwin_min_num, scratch[2]);
    ui_skinned_number_set (mainwin_10sec_num, scratch[4]);
    ui_skinned_number_set (mainwin_sec_num, scratch[5]);

    if (! ((UiSkinnedHorizontalSlider *) mainwin_sposition)->pressed)
    {
        textbox_set_text (mainwin_stime_min, scratch);
        textbox_set_text (mainwin_stime_sec, scratch + 4);
    }

    playlistwin_set_time (scratch, scratch + 4);
}

static void mainwin_update_time_slider (gint time, gint length)
{
    show_hide_widget (mainwin_position, length > 0);
    show_hide_widget (mainwin_sposition, length > 0);

    if (length > 0 && seek_source == 0)
    {
        if (time < length)
        {
            ui_skinned_horizontal_slider_set_position (mainwin_position, time *
             (gint64) 219 / length);
            ui_skinned_horizontal_slider_set_position (mainwin_sposition, 1 +
             time * (gint64) 12 / length);
        }
        else
        {
            ui_skinned_horizontal_slider_set_position (mainwin_position, 219);
            ui_skinned_horizontal_slider_set_position (mainwin_sposition, 13);
        }
    }
}

void mainwin_update_song_info (void)
{
    gint time, length;

    mainwin_update_volume ();

    if (! aud_drct_get_playing ())
        return;

    time = aud_drct_get_time ();
    length = aud_drct_get_length ();

    /* Ugh, this does NOT belong here. -jlindgren */
    if (ab_position_a > -1 && ab_position_b > -1 && time >= ab_position_b)
    {
        aud_drct_seek (ab_position_a);
        return;
    }

    mainwin_update_time_display (time, length);
    mainwin_update_time_slider (time, length);
}

/* toggleactionentries actions */

void action_anamode_peaks (GtkToggleAction * action)
{
    config.analyzer_peaks = gtk_toggle_action_get_active (action);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_autoscroll_songname (GtkToggleAction * action)
{
    config.autoscroll = gtk_toggle_action_get_active (action);
    textbox_set_scroll (mainwin_info, config.autoscroll);
    playlistwin_set_sinfo_scroll (config.autoscroll);
}

void action_playback_noplaylistadvance (GtkToggleAction * action)
{
    aud_cfg->no_playlist_advance = gtk_toggle_action_get_active( action );

    if (aud_cfg->no_playlist_advance)
        show_status_message (_("Single mode."));
    else
        show_status_message (_("Playlist mode."));
}

void action_playback_repeat (GtkToggleAction * action)
{
    aud_cfg->repeat = gtk_toggle_action_get_active( action );
    button_set_active (mainwin_repeat, aud_cfg->repeat);
}

void action_playback_shuffle (GtkToggleAction * action)
{
    aud_cfg->shuffle = gtk_toggle_action_get_active( action );
    button_set_active (mainwin_shuffle, aud_cfg->shuffle);
}

void action_stop_after_current_song (GtkToggleAction * action)
{
    gboolean active = gtk_toggle_action_get_active (action);

    if (active != aud_cfg->stopaftersong)
    {
        if (active)
            show_status_message (_("Stopping after song."));
        else
            show_status_message (_("Not stopping after song."));

        aud_cfg->stopaftersong = active;
        hook_call ("toggle stop after song", NULL);
    }
}

void action_view_always_on_top (GtkToggleAction * action)
{
    config.always_on_top = gtk_toggle_action_get_active (action);
    ui_skinned_menurow_update (mainwin_menurow);
    hint_set_always (config.always_on_top);
}

void action_view_easymove (GtkToggleAction * action)
{
    config.easy_move = gtk_toggle_action_get_active( action );
}

void action_view_on_all_workspaces (GtkToggleAction * action)
{
    config.sticky = gtk_toggle_action_get_active( action );
    hint_set_sticky(config.sticky);
}

void action_roll_up_player (GtkToggleAction * action)
{
    config.player_shaded = gtk_toggle_action_get_active (action);

    ui_skinned_window_set_shade (mainwin, config.player_shaded);
    mainwin_set_shape ();

    gint height;
    if (config.player_shaded)
        height = MAINWIN_SHADED_HEIGHT;
    else if (aud_active_skin->properties.mainwin_height)
        height = aud_active_skin->properties.mainwin_height;
    else
        height = MAINWIN_HEIGHT;

    dock_shade (get_dock_window_list (), (GtkWindow *) mainwin, height);
}

void action_show_player (GtkToggleAction * action)
{
    mainwin_show(gtk_toggle_action_get_active(action));
}


/* radioactionentries actions (one callback for each radio group) */

void action_anafoff (GtkAction * action, GtkRadioAction * current)
{
    config.analyzer_falloff = gtk_radio_action_get_current_value (current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_anamode (GtkAction * action, GtkRadioAction * current)
{
    config.analyzer_mode = gtk_radio_action_get_current_value (current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_anatype (GtkAction * action, GtkRadioAction * current)
{
    config.analyzer_type = gtk_radio_action_get_current_value (current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_peafoff (GtkAction * action, GtkRadioAction * current)
{
    config.peaks_falloff = gtk_radio_action_get_current_value (current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_scomode (GtkAction * action, GtkRadioAction * current)
{
    config.scope_mode = gtk_radio_action_get_current_value (current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_vismode (GtkAction * action, GtkRadioAction * current)
{
    config.vis_type = gtk_radio_action_get_current_value (current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);

    start_stop_visual (FALSE);
}

void action_vprmode (GtkAction * action, GtkRadioAction * current)
{
    config.voiceprint_mode = gtk_radio_action_get_current_value(current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_wshmode (GtkAction * action, GtkRadioAction * current)
{
    config.vu_mode = gtk_radio_action_get_current_value(current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_viewtime (GtkAction * action, GtkRadioAction * current)
{
    config.timer_mode = gtk_radio_action_get_current_value (current);
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
        if (ab_position_a == -1)
        {
            ab_position_a = aud_drct_get_time();
            ab_position_b = -1;
            mainwin_lock_info_text("LOOP-POINT A POSITION SET.");
        }
        else if (ab_position_b == -1)
        {
            int time = aud_drct_get_time();
            if (time > ab_position_a)
                ab_position_b = time;
            mainwin_release_info_text();
        }
        else
        {
            ab_position_a = aud_drct_get_time();
            ab_position_b = -1;
            mainwin_lock_info_text("LOOP-POINT A POSITION RESET.");
        }
    }
}

void action_ab_clear (void)
{
    if (aud_drct_get_length () > 0)
    {
        ab_position_a = ab_position_b = -1;
        mainwin_release_info_text();
    }
}
