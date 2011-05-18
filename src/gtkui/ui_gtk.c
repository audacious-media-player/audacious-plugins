/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team
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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <audacious/audconfig.h>
#include <audacious/debug.h>
#include <audacious/drct.h>
#include <audacious/gtk-compat.h>
#include <audacious/i18n.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <audacious/misc.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"
#include "gtkui.h"
#include "gtkui_cfg.h"
#include "layout.h"
#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"
#include "ui_infoarea.h"
#include "ui_statusbar.h"
#include "playlist_util.h"

#if GTK_CHECK_VERSION (2, 12, 0)
#define HAVE_VOLUME
static GtkWidget *volume;
static gboolean volume_slider_is_moving = FALSE;
static guint update_volume_timeout_source = 0;
static gulong volume_change_handler_id;
#endif

static GtkWidget * button_play, * button_pause, * button_stop, * slider,
 * label_time, * button_shuffle, * button_repeat;
static GtkWidget * playlist_box, * window, * vbox, * menu, * infoarea,
 * statusbar;
static GtkWidget * menu_rclick, * menu_tab;

static GtkWidget * error_win = NULL;

static gulong slider_change_handler_id;
static gboolean slider_is_moving = FALSE;
static gint slider_position;
static guint delayed_title_change_source = 0;
static guint update_song_timeout_source = 0;

extern GtkWidget *ui_playlist_notebook_tab_title_editing;

static gboolean init (void);
static void cleanup (void);
static void ui_show (gboolean show);
static gboolean ui_is_shown (void);
static void ui_show_error (const gchar * text);

AUD_IFACE_PLUGIN
(
    .name = N_("GTK Interface"),
    .init = init,
    .cleanup = cleanup,
    .show = ui_show,
    .is_shown = ui_is_shown,
    .show_error = ui_show_error,
    .show_filebrowser = audgui_run_filebrowser,
    .show_jump_to_track = audgui_jump_to_track,
    .run_gtk_plugin = (void *) layout_add,
    .stop_gtk_plugin = (void *) layout_remove,
)

static void save_window_size (void)
{
    gtk_window_get_position ((GtkWindow *) window, & config.player_x,
     & config.player_y);

    if (gtk_window_get_resizable ((GtkWindow *) window))
        gtk_window_get_size ((GtkWindow *) window, & config.player_width,
         & config.player_height);
}

static gboolean window_delete()
{
    gboolean handle = FALSE;

    hook_call("window close", &handle);

    if (handle)
        return TRUE;

    aud_drct_quit ();
    return TRUE;
}

static void button_open_pressed()
{
    audgui_run_filebrowser(TRUE);
}

static void button_add_pressed()
{
    audgui_run_filebrowser(FALSE);
}

static gboolean title_change_cb (void)
{
    if (delayed_title_change_source)
    {
        g_source_remove (delayed_title_change_source);
        delayed_title_change_source = 0;
    }

    if (aud_drct_get_playing () && config.show_song_titles)
    {
        gchar * title = aud_drct_get_title ();
        gchar * title_s = g_strdup_printf (_("%s - Audacious"), title);
        gtk_window_set_title ((GtkWindow *) window, title_s);
        g_free (title_s);
        g_free (title);
    }
    else
        gtk_window_set_title ((GtkWindow *) window, _("Audacious"));

    return FALSE;
}

static void ui_show (gboolean show)
{
    config.player_visible = show;

    if (show)
    {
        if (config.save_window_position && ! gtk_widget_get_visible (window))
            gtk_window_move ((GtkWindow *) window, config.player_x,
             config.player_y);

        gtk_window_present ((GtkWindow *) window);
    }
    else if (gtk_widget_get_visible (window))
    {
        if (config.save_window_position)
            gtk_window_get_position ((GtkWindow *) window, & config.player_x,
             & config.player_y);

        gtk_widget_hide (window);
    }
}

static gboolean ui_is_shown (void)
{
    return config.player_visible;
}

static void ui_show_error (const gchar * text)
{
    audgui_simple_message (& error_win, GTK_MESSAGE_ERROR, _("Error"), _(text));
}

static void set_time_label (gint time, gint len)
{
    gchar s[128];
    snprintf (s, sizeof s, "<tt><b>");

    time /= 1000;

    if (time < 3600)
        snprintf (s + strlen (s), sizeof s - strlen (s), aud_cfg->leading_zero ?
         "%02d:%02d" : "%d:%02d", time / 60, time % 60);
    else
        snprintf (s + strlen (s), sizeof s - strlen (s), "%d:%02d:%02d", time /
         3600, (time / 60) % 60, time % 60);

    if (len)
    {
        len /= 1000;

        if (len < 3600)
            snprintf (s + strlen (s), sizeof s - strlen (s),
             aud_cfg->leading_zero ? "/%02d:%02d" : "/%d:%02d", len / 60, len %
             60);
        else
            snprintf (s + strlen (s), sizeof s - strlen (s), "/%d:%02d:%02d",
             len / 3600, (len / 60) % 60, len % 60);
    }

    snprintf (s + strlen (s), sizeof s - strlen (s), "</b></tt>");
    gtk_label_set_markup ((GtkLabel *) label_time, s);
}

static void set_slider (gint time)
{
    if (g_signal_handler_is_connected (slider, slider_change_handler_id))
        g_signal_handler_block (slider, slider_change_handler_id);

    gtk_range_set_value ((GtkRange *) slider, time);

    if (g_signal_handler_is_connected (slider, slider_change_handler_id))
        g_signal_handler_unblock (slider, slider_change_handler_id);
}

static gboolean time_counter_cb (void)
{
    if (slider_is_moving)
        return TRUE;

    gint time = aud_drct_get_time ();
    gint length = aud_drct_get_length ();

    if (length > 0)
        set_slider (time);

    set_time_label (time, length);

    return TRUE;
}

static gboolean ui_slider_value_changed_cb(GtkRange * range, gpointer user_data)
{
    aud_drct_seek (gtk_range_get_value (range));
    slider_is_moving = FALSE;
    return TRUE;
}

static gboolean ui_slider_change_value_cb(GtkRange * range, GtkScrollType scroll)
{
    set_time_label (gtk_range_get_value (range), aud_drct_get_length ());
    return FALSE;
}

static gboolean ui_slider_button_press_cb(GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
    slider_is_moving = TRUE;
    slider_position = gtk_range_get_value(GTK_RANGE(widget));

    /* HACK: clicking with the left mouse button moves the slider
       to the location of the click. */
    if (event->button == 1)
        event->button = 2;

    return FALSE;
}

static gboolean ui_slider_button_release_cb(GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
    /* HACK: see ui_slider_button_press_cb */
    if (event->button == 1)
        event->button = 2;

    if (slider_position == (gint) gtk_range_get_value(GTK_RANGE(widget)))
        slider_is_moving = FALSE;

    return FALSE;
}

#ifdef HAVE_VOLUME
static gboolean ui_volume_value_changed_cb(GtkButton * button, gdouble volume, gpointer user_data)
{
    aud_drct_set_volume((gint) volume, (gint) volume);

    return TRUE;
}

static void ui_volume_pressed_cb(GtkButton *button, gpointer user_data)
{
    volume_slider_is_moving = TRUE;
}

static void ui_volume_released_cb(GtkButton *button, gpointer user_data)
{
    volume_slider_is_moving = FALSE;
}

static gboolean ui_volume_slider_update(gpointer data)
{
    gint volume;
    static gint last_volume = -1;

    if (volume_slider_is_moving || data == NULL)
        return TRUE;

    aud_drct_get_volume_main(&volume);

    if (last_volume == volume)
        return TRUE;

    last_volume = volume;

    if (volume == (gint) gtk_scale_button_get_value(GTK_SCALE_BUTTON(data)))
        return TRUE;

    g_signal_handler_block(data, volume_change_handler_id);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(data), volume);
    g_signal_handler_unblock(data, volume_change_handler_id);

    return TRUE;
}
#endif

static void set_slider_length (gint length)
{
    if (g_signal_handler_is_connected (slider, slider_change_handler_id))
        g_signal_handler_block (slider, slider_change_handler_id);

    if (length > 0)
    {
        gtk_range_set_range ((GtkRange *) slider, 0, length);
        gtk_widget_show (slider);
    }
    else
        gtk_widget_hide (slider);

    if (g_signal_handler_is_connected (slider, slider_change_handler_id))
        g_signal_handler_unblock (slider, slider_change_handler_id);
}

static void pause_cb (void)
{
    if (aud_drct_get_paused ())
    {
        gtk_widget_show (button_play);
        gtk_widget_hide (button_pause);
    }
    else
    {
        gtk_widget_hide (button_play);
        gtk_widget_show (button_pause);
    }
}

static void ui_playback_begin(gpointer hook_data, gpointer user_data)
{
    pause_cb ();
    gtk_widget_set_sensitive (button_stop, TRUE);

    if (delayed_title_change_source)
        g_source_remove (delayed_title_change_source);

    /* If "title change" is not called by 1/4 second after starting playback,
     * show "Buffering ..." as the window title. */
    delayed_title_change_source = g_timeout_add (250, (GSourceFunc)
     title_change_cb, NULL);

    set_slider_length (aud_drct_get_length ());
    time_counter_cb ();

    /* update time counter 4 times a second */
    if (! update_song_timeout_source)
        update_song_timeout_source = g_timeout_add (250, (GSourceFunc)
         time_counter_cb, NULL);

    gtk_widget_show (label_time);
}

static void ui_playback_stop(gpointer hook_data, gpointer user_data)
{
    if (update_song_timeout_source)
    {
        g_source_remove(update_song_timeout_source);
        update_song_timeout_source = 0;
    }

    if (delayed_title_change_source)
        g_source_remove (delayed_title_change_source);

    /* Don't update the window title immediately; we may be about to start
     * another song. */
    delayed_title_change_source = g_idle_add ((GSourceFunc) title_change_cb,
     NULL);

    gtk_widget_show (button_play);
    gtk_widget_hide (button_pause);
    gtk_widget_set_sensitive (button_stop, FALSE);
    gtk_widget_hide (slider);
    gtk_widget_hide (label_time);
}

static GtkWidget *gtk_toolbar_button_add(GtkWidget * toolbar, void (*callback) (), const gchar * stock_id)
{
    GtkWidget *icon;
    GtkWidget *button = gtk_button_new();

    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(button, FALSE);

    icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(button), icon);

    gtk_box_pack_start(GTK_BOX(toolbar), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(callback), NULL);

    return button;
}

static GtkWidget * toggle_button_new (const gchar * icon, const gchar * alt,
 void (* toggled) (GtkToggleButton * button, void * user), void * user)
{
    GtkWidget * button = gtk_toggle_button_new ();
    gtk_widget_set_can_focus (button, FALSE);
    gtk_button_set_relief ((GtkButton *) button, GTK_RELIEF_NONE);

    if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), icon))
        gtk_container_add ((GtkContainer *) button, gtk_image_new_from_icon_name
         (icon, GTK_ICON_SIZE_BUTTON));
    else
    {
        GtkWidget * label = gtk_label_new (NULL);
        gchar * markup = g_markup_printf_escaped ("<small>%s</small>", alt);
        gtk_label_set_markup ((GtkLabel *) label, markup);
        g_free (markup);
        gtk_container_add ((GtkContainer *) button, label);
    }

    g_signal_connect (button, "toggled", (GCallback) toggled, user);
    return button;
}

static GtkWidget *gtk_markup_label_new(const gchar * str)
{
    GtkWidget *label = gtk_label_new(str);
    g_object_set(G_OBJECT(label), "use-markup", TRUE, NULL);
    return label;
}

static gboolean ui_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    if (ui_playlist_notebook_tab_title_editing)
    {
        if (event->keyval == GDK_KP_Enter || event->keyval == GDK_Escape)
            return FALSE;

        GtkWidget *entry = g_object_get_data(G_OBJECT(ui_playlist_notebook_tab_title_editing), "entry");
        gtk_widget_event(entry, (GdkEvent*) event);
        return TRUE;
    }

    switch (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))
    {
        case 0:
            switch (event->keyval)
            {
                case GDK_Left:
                case GDK_KP_Left:
                case GDK_KP_7:
                    if (aud_drct_get_playing ())
                        aud_drct_seek (aud_drct_get_time () - 5000);
                    break;

                case GDK_Right:
                case GDK_KP_Right:
                case GDK_KP_9:
                    if (aud_drct_get_playing ())
                        aud_drct_seek (aud_drct_get_time () + 5000);
                    break;

                case GDK_KP_4:
                    aud_drct_pl_prev();
                    break;

                case GDK_KP_6:
                    aud_drct_pl_next();
                    break;

                case GDK_space:
                    if (aud_drct_get_playing())
                        aud_drct_pause();
                    else
                        aud_drct_play();
                    break;

                case GDK_Escape:
                    ui_playlist_notebook_position (GINT_TO_POINTER
                     (aud_playlist_get_active ()), NULL);
                    break;

                case GDK_Delete:
                    playlist_delete_selected ();
                    break;

                case GDK_Menu:
                    popup_menu_rclick (0, event->time);
                    break;

                default:
                    return FALSE;
            }
            break;

      case GDK_CONTROL_MASK:
        switch (event->keyval)
        {
          case GDK_ISO_Left_Tab:
          case GDK_Tab:
            aud_playlist_set_active ((aud_playlist_get_active () + 1) %
             aud_playlist_count ());
            break;
          default:
            return FALSE;
        }
        break;
      case (GDK_CONTROL_MASK | GDK_SHIFT_MASK):
        switch (event->keyval)
        {
          case GDK_ISO_Left_Tab:
          case GDK_Tab:
            aud_playlist_set_active (aud_playlist_get_active () ?
             aud_playlist_get_active () - 1 : aud_playlist_count () - 1);
            break;
          default:
            return FALSE;
        }
        break;
      default:
        return FALSE;
    }

    return TRUE;
}

static void update_toggles (void * data, void * user)
{
    if (gtk_toggle_button_get_active ((GtkToggleButton *) button_repeat) != aud_cfg->repeat)
        gtk_toggle_button_set_active ((GtkToggleButton *) button_repeat, aud_cfg->repeat);
    if (gtk_toggle_button_get_active ((GtkToggleButton *) button_shuffle) != aud_cfg->shuffle)
        gtk_toggle_button_set_active ((GtkToggleButton *) button_shuffle, aud_cfg->shuffle);
}

static void toggle_repeat (GtkToggleButton * button, void * unused)
{
    if (aud_cfg->repeat != gtk_toggle_button_get_active (button))
    {
        aud_cfg->repeat = gtk_toggle_button_get_active (button);
        hook_call ("toggle repeat", NULL);
    }
}

static void toggle_shuffle (GtkToggleButton * button, void * unused)
{
    if (aud_cfg->shuffle != gtk_toggle_button_get_active (button))
    {
        aud_cfg->shuffle = gtk_toggle_button_get_active (button);
        hook_call ("toggle shuffle", NULL);
    }
}

static void config_save (void)
{
    save_window_size ();
    layout_save ();
    pw_col_save ();
    gtkui_cfg_save ();
}

static void ui_hooks_associate(void)
{
    hook_associate ("title change", (HookFunction) title_change_cb, NULL);
    hook_associate ("playback seek", (HookFunction) time_counter_cb, NULL);
    hook_associate("playback begin", ui_playback_begin, NULL);
    hook_associate ("playback pause", (HookFunction) pause_cb, NULL);
    hook_associate ("playback unpause", (HookFunction) pause_cb, NULL);
    hook_associate("playback stop", ui_playback_stop, NULL);
    hook_associate("playlist update", ui_playlist_notebook_update, NULL);
    hook_associate ("playlist activate", ui_playlist_notebook_activate, NULL);
    hook_associate ("playlist position", ui_playlist_notebook_position, NULL);
    hook_associate ("toggle stop after song", update_toggles, NULL);
    hook_associate ("toggle shuffle", update_toggles, NULL);
    hook_associate ("toggle repeat", update_toggles, NULL);
    hook_associate ("config save", (HookFunction) config_save, NULL);
}

static void ui_hooks_disassociate(void)
{
    hook_dissociate ("title change", (HookFunction) title_change_cb);
    hook_dissociate ("playback seek", (HookFunction) time_counter_cb);
    hook_dissociate("playback begin", ui_playback_begin);
    hook_dissociate ("playback pause", (HookFunction) pause_cb);
    hook_dissociate ("playback unpause", (HookFunction) pause_cb);
    hook_dissociate("playback stop", ui_playback_stop);
    hook_dissociate("playlist update", ui_playlist_notebook_update);
    hook_dissociate ("playlist activate", ui_playlist_notebook_activate);
    hook_dissociate ("playlist position", ui_playlist_notebook_position);
    hook_dissociate ("toggle stop after song", update_toggles);
    hook_dissociate ("toggle shuffle", update_toggles);
    hook_dissociate ("toggle repeat", update_toggles);
    hook_dissociate ("config save", (HookFunction) config_save);
}

static gboolean init (void)
{
    GtkWidget *tophbox;         /* box to contain toolbar and shbox */
    GtkWidget *buttonbox;       /* contains buttons like "open", "next" */
    GtkWidget *shbox;           /* box for volume control + slider + time combo --nenolod */
    GtkWidget *evbox;

    gtkui_cfg_load();

    audgui_set_default_icon();
    audgui_register_stock_icons();

    pw_col_init ();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), MAINWIN_DEFAULT_WIDTH, MAINWIN_DEFAULT_HEIGHT);
    gtk_window_set_keep_above(GTK_WINDOW(window), config.always_on_top);

    if (config.save_window_position && config.player_width && config.player_height)
        gtk_window_resize(GTK_WINDOW(window), config.player_width, config.player_height);

    if (config.save_window_position && config.player_x != -1)
        gtk_window_move(GTK_WINDOW(window), config.player_x, config.player_y);
    else
        gtk_window_move(GTK_WINDOW(window), MAINWIN_DEFAULT_POS_X, MAINWIN_DEFAULT_POS_Y);

    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(window_delete), NULL);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkAccelGroup * accel = gtk_accel_group_new ();
    gtk_window_add_accel_group ((GtkWindow *) window, accel);

    menu = make_menu_bar (accel);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, TRUE, 0);

    tophbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), tophbox, FALSE, TRUE, 0);

    buttonbox = gtk_hbox_new(FALSE, 0);
    gtk_toolbar_button_add(buttonbox, button_open_pressed, GTK_STOCK_OPEN);
    gtk_toolbar_button_add(buttonbox, button_add_pressed, GTK_STOCK_ADD);
    button_play = gtk_toolbar_button_add (buttonbox, aud_drct_play, GTK_STOCK_MEDIA_PLAY);
    button_pause = gtk_toolbar_button_add (buttonbox, aud_drct_pause, GTK_STOCK_MEDIA_PAUSE);
    button_stop = gtk_toolbar_button_add (buttonbox, aud_drct_stop, GTK_STOCK_MEDIA_STOP);
    gtk_toolbar_button_add (buttonbox, aud_drct_pl_prev, GTK_STOCK_MEDIA_PREVIOUS);
    gtk_toolbar_button_add (buttonbox, aud_drct_pl_next, GTK_STOCK_MEDIA_NEXT);

    /* Workaround: Show the play and pause buttons and then hide them again in
     * order to coax GTK into loading icons for them. -jlindgren */
    gtk_widget_show_all (button_play);
    gtk_widget_show_all (button_pause);
    gtk_widget_hide (button_play);
    gtk_widget_hide (button_pause);

    gtk_widget_set_no_show_all (button_play, TRUE);
    gtk_widget_set_no_show_all (button_pause, TRUE);

    gtk_box_pack_start(GTK_BOX(tophbox), buttonbox, FALSE, FALSE, 0);

    /* The slider and time display are packed in an event box so that they can
     * be redrawn separately as they are updated. */
    evbox = gtk_event_box_new ();
    gtk_box_pack_start ((GtkBox *) tophbox, evbox, TRUE, TRUE, 0);

    shbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add ((GtkContainer *) evbox, shbox);

    slider = gtk_hscale_new(NULL);
    gtk_scale_set_draw_value(GTK_SCALE(slider), FALSE);
    gtk_range_set_update_policy(GTK_RANGE(slider), GTK_UPDATE_DISCONTINUOUS);
    gtk_widget_set_size_request(slider, 120, -1);
    gtk_widget_set_can_focus(slider, FALSE);
    gtk_box_pack_start ((GtkBox *) shbox, slider, TRUE, TRUE, 6);
    gtk_widget_set_no_show_all (slider, TRUE);

    label_time = gtk_markup_label_new(NULL);
    gtk_widget_set_no_show_all (label_time, TRUE);

    gtk_box_pack_end ((GtkBox *) shbox, label_time, FALSE, FALSE, 6);

    /* XXX: LOL REALLY BAD HACK FOR MOODBAR PLUGIN. Remove once AUD-283 is solved.
     * Because, I mean, who cares if this stuff is supposed to be final. --nenolod */
    mowgli_global_storage_put("gtkui.shbox", shbox);
    mowgli_global_storage_put("gtkui.slider", slider);
    mowgli_global_storage_put("gtkui.label_time", label_time);

#ifdef HAVE_VOLUME
    volume = gtk_volume_button_new();
    gtk_button_set_relief(GTK_BUTTON(volume), GTK_RELIEF_NONE);
    gtk_scale_button_set_adjustment(GTK_SCALE_BUTTON(volume), GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 100, 1, 5, 0)));
    gtk_widget_set_can_focus(volume, FALSE);
    /* Set the default volume to the balance average.
       (I'll add balance control later) -Ryan */
    gint lvol = 0, rvol = 0;
    aud_drct_get_volume(&lvol, &rvol);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume), (lvol + rvol) / 2);

    gtk_box_pack_end ((GtkBox *) tophbox, volume, FALSE, FALSE, 0);
#endif

    button_shuffle = toggle_button_new ("media-playlist-shuffle", "SHUF",
     toggle_shuffle, NULL);
    gtk_box_pack_end ((GtkBox *) tophbox, button_shuffle, FALSE, FALSE, 0);
    button_repeat = toggle_button_new ("media-playlist-repeat", "REP",
     toggle_repeat, NULL);
    gtk_box_pack_end ((GtkBox *) tophbox, button_repeat, FALSE, FALSE, 0);

    playlist_box = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), playlist_box, TRUE, TRUE, 0);

    /* Create playlist notebook */
    ui_playlist_notebook_new ();
    g_object_ref (G_OBJECT(UI_PLAYLIST_NOTEBOOK));

    if (config.statusbar_visible)
    {
        AUDDBG("statusbar setup\n");
        statusbar = ui_statusbar_new();
        gtk_box_pack_end(GTK_BOX(vbox), statusbar, FALSE, FALSE, 3);
    }

    layout_load ();

    GtkWidget * layout = layout_new ();
    gtk_box_pack_start ((GtkBox *) playlist_box, layout, TRUE, TRUE, 0);
    layout_add_center ((GtkWidget *) UI_PLAYLIST_NOTEBOOK);

    if (config.infoarea_visible)
    {
        AUDDBG ("infoarea setup\n");
        infoarea = ui_infoarea_new ();
        gtk_box_pack_end (GTK_BOX(vbox), infoarea, FALSE, FALSE, 0);
    }

    AUDDBG("hooks associate\n");
    ui_hooks_associate();

    AUDDBG("playlist associate\n");
    ui_playlist_notebook_populate();

    slider_change_handler_id = g_signal_connect(slider, "value-changed", G_CALLBACK(ui_slider_value_changed_cb), NULL);

    g_signal_connect(slider, "change-value", G_CALLBACK(ui_slider_change_value_cb), NULL);
    g_signal_connect(slider, "button-press-event", G_CALLBACK(ui_slider_button_press_cb), NULL);
    g_signal_connect(slider, "button-release-event", G_CALLBACK(ui_slider_button_release_cb), NULL);

#ifdef HAVE_VOLUME
    volume_change_handler_id = g_signal_connect(volume, "value-changed", G_CALLBACK(ui_volume_value_changed_cb), NULL);
    g_signal_connect(volume, "pressed", G_CALLBACK(ui_volume_pressed_cb), NULL);
    g_signal_connect(volume, "released", G_CALLBACK(ui_volume_released_cb), NULL);
    update_volume_timeout_source = g_timeout_add(250, (GSourceFunc) ui_volume_slider_update, volume);
#endif

    g_signal_connect(window, "key-press-event", G_CALLBACK(ui_key_press_cb), NULL);

    if (!config.menu_visible)
        gtk_widget_hide(menu);

    if (aud_drct_get_playing())
        ui_playback_begin(NULL, NULL);
    else
        ui_playback_stop (NULL, NULL);

    title_change_cb ();

    gtk_widget_show_all (vbox);

    if (config.player_visible)
        ui_show (TRUE);

    update_toggles (NULL, NULL);

    menu_rclick = make_menu_rclick (accel);
    menu_tab = make_menu_tab (accel);

    return TRUE;
}

static void cleanup (void)
{
    if (error_win)
        gtk_widget_destroy (error_win);

    gtk_widget_destroy (menu_rclick);
    gtk_widget_destroy (menu_tab);

    if (update_song_timeout_source)
    {
        g_source_remove(update_song_timeout_source);
        update_song_timeout_source = 0;
    }

#ifdef HAVE_VOLUME
    if (update_volume_timeout_source)
    {
        g_source_remove(update_volume_timeout_source);
        update_volume_timeout_source = 0;
    }
#endif

    if (delayed_title_change_source)
    {
        g_source_remove (delayed_title_change_source);
        delayed_title_change_source = 0;
    }

    gtkui_cfg_free();
    ui_hooks_disassociate();

    pw_col_cleanup ();

    g_object_unref ((GObject *) UI_PLAYLIST_NOTEBOOK);
    gtk_widget_destroy (window);

    layout_cleanup ();
}

void show_menu (gboolean show)
{
    config.menu_visible = show;

    if (config.menu_visible)
        gtk_widget_show (menu);
    else
        gtk_widget_hide (menu);
}

void show_infoarea (gboolean show)
{
    config.infoarea_visible = show;

    if (config.infoarea_visible && ! infoarea)
    {
        infoarea = ui_infoarea_new ();
        gtk_box_pack_end ((GtkBox *) vbox, infoarea, FALSE, FALSE, 0);
        gtk_box_reorder_child ((GtkBox *) vbox, infoarea, -1);
        gtk_widget_show (infoarea);
    }

    if (! config.infoarea_visible && infoarea)
    {
        gtk_widget_destroy (infoarea);
        infoarea = NULL;
    }
}

void show_statusbar (gboolean show)
{
    config.statusbar_visible = show;

    if (config.statusbar_visible && ! statusbar)
    {
        statusbar = ui_statusbar_new ();
        gtk_box_pack_end ((GtkBox *) vbox, statusbar, FALSE, FALSE, 3);

        if (infoarea)
            gtk_box_reorder_child ((GtkBox *) vbox, infoarea, -1);

        gtk_widget_show_all (statusbar);
    }

    if (! config.statusbar_visible && statusbar)
    {
        gtk_widget_destroy (statusbar);
        statusbar = NULL;
    }
}

void popup_menu_rclick (guint button, guint32 time)
{
    gtk_menu_popup ((GtkMenu *) menu_rclick, NULL, NULL, NULL, NULL, button,
     time);
}

void popup_menu_tab (guint button, guint32 time)
{
    gtk_menu_popup ((GtkMenu *) menu_tab, NULL, NULL, NULL, NULL, button, time);
}
