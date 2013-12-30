/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2008 Tomasz Moń
 * Copyright (c) 2011 John Lindgren
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

#include <string.h>
#include <gtk/gtk.h>

#include <audacious/debug.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/preferences.h>
#include <libaudcore/audstrings.h>

#include "dnd.h"
#include "skins_cfg.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_playlist.h"
#include "ui_skin.h"
#include "ui_skinned_playlist.h"
#include "ui_skinned_textbox.h"
#include "ui_skinselector.h"
#include "ui_vis.h"
#include "util.h"

static const gchar * const skins_defaults[] = {
 /* general */
 "autoscroll_songname", "TRUE",
 "mainwin_font", "Sans Bold 9",
 "mainwin_use_bitmapfont", "TRUE",
 "playlist_font", "Sans Bold 8",
 "timer_mode", "0", /* TIMER_ELAPSED */
 "twoway_scroll", "FALSE",

 /* visualizer */
 "analyzer_falloff", "3", /* FALLOFF_FAST */
 "analyzer_mode", "0", /* ANALYZER_NORMAL */
 "analyzer_peaks", "TRUE",
 "analyzer_type", "1", /* ANALYZER_BARS */
 "peaks_falloff", "1", /* FALLOFF_SLOW */
 "scope_mode", "0", /* SCOPE_DOT */
 "vis_type", "0", /* VIS_ANALYZER */
 "voiceprint_mode", "0", /* VOICEPRINT_NORMAL */
 "vu_mode", "1", /* VU_SMOOTH */

 /* windows */
 "always_on_top", "FALSE",
 "equalizer_shaded", "FALSE",
 "equalizer_visible", "FALSE",
 "equalizer_x", "20",
 "equalizer_y", "136",
 "player_shaded", "FALSE",
 "player_x", "20",
 "player_y", "20",
 "playlist_shaded", "FALSE",
 "playlist_visible", "FALSE",
 "playlist_x", "295",
 "playlist_y", "20",
 "playlist_width", "275",
 "playlist_height", "232",
 "sticky", "FALSE",
 NULL};

skins_cfg_t config;

static GtkWidget * skin_view;

typedef struct skins_cfg_boolent_t {
    const gchar * name;
    gboolean * ptr;
} skins_cfg_boolent;

static const skins_cfg_boolent skins_boolents[] = {
 /* general */
 {"autoscroll_songname", & config.autoscroll},
 {"mainwin_use_bitmapfont", & config.mainwin_use_bitmapfont},
 {"twoway_scroll", & config.twoway_scroll},

 /* visualizer */
 {"analyzer_peaks", & config.analyzer_peaks},

 /* windows */
 {"always_on_top", & config.always_on_top},
 {"equalizer_shaded", & config.equalizer_shaded},
 {"equalizer_visible", & config.equalizer_visible},
 {"player_shaded", & config.player_shaded},
 {"playlist_shaded", & config.playlist_shaded},
 {"playlist_visible", & config.playlist_visible},
 {"sticky", & config.sticky}};

typedef struct skins_cfg_nument_t {
    const gchar * name;
    gint * ptr;
} skins_cfg_nument;

static const skins_cfg_nument skins_numents[] = {
 /* general */
 {"timer_mode", & config.timer_mode},

 /* visualizer */
 {"analyzer_falloff", & config.analyzer_falloff},
 {"analyzer_mode", & config.analyzer_mode},
 {"analyzer_type", & config.analyzer_type},
 {"peaks_falloff", & config.peaks_falloff},
 {"scope_mode", & config.scope_mode},
 {"vis_type", & config.vis_type},
 {"voiceprint_mode", & config.voiceprint_mode},
 {"vu_mode", & config.vu_mode},

 /* windows */
 {"equalizer_x", & config.equalizer_x},
 {"equalizer_y", & config.equalizer_y},
 {"player_x", & config.player_x},
 {"player_y", & config.player_y},
 {"playlist_x", & config.playlist_x},
 {"playlist_y", & config.playlist_y},
 {"playlist_width", & config.playlist_width},
 {"playlist_height", & config.playlist_height}};

typedef struct skins_cfg_strent_t {
    const gchar * name;
    gchar * * ptr;
} skins_cfg_strent;

void skins_cfg_load (void)
{
    aud_config_set_defaults ("skins", skins_defaults);

    for (gint i = 0; i < ARRAY_LEN (skins_boolents); i ++)
        * skins_boolents[i].ptr = aud_get_bool ("skins", skins_boolents[i].name);

    for (gint i = 0; i < ARRAY_LEN (skins_numents); i ++)
        * skins_numents[i].ptr = aud_get_int ("skins", skins_numents[i].name);
}

void skins_cfg_save (void)
{
    for (gint i = 0; i < ARRAY_LEN (skins_boolents); i ++)
        aud_set_bool ("skins", skins_boolents[i].name, * skins_boolents[i].ptr);

    for (gint i = 0; i < ARRAY_LEN (skins_numents); i ++)
        aud_set_int ("skins", skins_numents[i].name, * skins_numents[i].ptr);
}

static void
mainwin_font_set_cb()
{
    char * font = aud_get_str ("skins", "mainwin_font");
    textbox_set_font (mainwin_info, config.mainwin_use_bitmapfont ? NULL : font);
    str_unref (font);
}

static void
playlist_font_set_cb()
{
    char * font = aud_get_str ("skins", "playlist_font");
    ui_skinned_playlist_set_font (playlistwin_list, font);
    str_unref (font);
}

static PreferencesWidget font_table_elements[] = {
 {WIDGET_FONT_BTN, N_("_Player:"),
  .cfg_type = VALUE_STRING, .csect = "skins", .cname = "mainwin_font",
  .callback = mainwin_font_set_cb, .data = {.font_btn = {N_("Select main player window font:")}}},
 {WIDGET_FONT_BTN, N_("_Playlist:"),
  .cfg_type = VALUE_STRING, .csect = "skins", .cname = "playlist_font",
  .callback = playlist_font_set_cb, .data = {.font_btn = {N_("Select playlist font:")}}}};

static void * create_skin_view (void);

static PreferencesWidget appearance_misc_widgets[] = {
    {WIDGET_LABEL, N_("<b>Skin</b>")},
    {WIDGET_CUSTOM, .data.populate = create_skin_view},
    {WIDGET_LABEL, N_("<b>Fonts</b>")},
    {WIDGET_TABLE, .child = TRUE,
     .data.table = {font_table_elements, ARRAY_LEN (font_table_elements)}},
    {WIDGET_CHK_BTN, N_("Use bitmap fonts (supports ASCII only)"),
     .cfg_type = VALUE_BOOLEAN, .cfg = & config.mainwin_use_bitmapfont, .callback = mainwin_font_set_cb},
    {WIDGET_CHK_BTN, N_("Scroll song title in both directions"),
     .cfg_type = VALUE_BOOLEAN, .cfg = & config.twoway_scroll, .callback = textbox_update_all}};

void
on_skin_view_drag_data_received(GtkWidget * widget,
                                GdkDragContext * context,
                                gint x, gint y,
                                GtkSelectionData * selection_data,
                                guint info, guint time,
                                gpointer user_data)
{
    const gchar * data = (const gchar *) gtk_selection_data_get_data (selection_data);
    g_return_if_fail (data);

    const gchar * end = strchr (data, '\r');
    if (! end) end = strchr (data, '\n');
    if (! end) end = data + strlen (data);

    char * path = str_nget (data, end - data);

    if (strstr (path, "://"))
    {
        char * path2 = uri_to_filename (path);
        if (path2)
        {
            str_unref (path);
            path = path2;
        }
    }

    if (file_is_archive(path))
    {
        if (! active_skin_load (path))
            goto DONE;

        skin_install_skin(path);

        if (skin_view)
            skin_view_update ((GtkTreeView *) skin_view);
    }

DONE:
    str_unref (path);
}

static void * create_skin_view (void)
{
    GtkWidget * scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scrolled,
     GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scrolled, GTK_SHADOW_IN);
    gtk_widget_set_size_request (scrolled, -1, 160);

    skin_view = gtk_tree_view_new ();
    skin_view_realize ((GtkTreeView *) skin_view);
    skin_view_update ((GtkTreeView *) skin_view);
    gtk_container_add ((GtkContainer *) scrolled, skin_view);

    drag_dest_set (skin_view);

    g_signal_connect (skin_view, "drag-data-received",
     (GCallback) on_skin_view_drag_data_received, NULL);
    g_signal_connect (skin_view, "destroy", (GCallback) gtk_widget_destroyed, & skin_view);

    return scrolled;
}

static GtkWidget * config_window = NULL;

void skins_configure (void)
{
    if (config_window)
    {
        gtk_window_present ((GtkWindow *) config_window);
        return;
    }

    GtkWidget * appearance_page_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    aud_create_widgets ((GtkBox *) appearance_page_vbox,
     appearance_misc_widgets, ARRAY_LEN (appearance_misc_widgets));

    GtkWidget * hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start ((GtkBox *) appearance_page_vbox, hbox, FALSE, FALSE, 0);

    GtkWidget * button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
    g_signal_connect (button, "clicked", (GCallback) skins_configure_cleanup,
     NULL);
    gtk_widget_set_can_default (button, TRUE);
    gtk_box_pack_end ((GtkBox *) hbox, button, FALSE, FALSE, 0);

    config_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (config_window, "destroy", (GCallback)
     gtk_widget_destroyed, & config_window);

    gtk_window_set_type_hint ((GtkWindow *) config_window,
     GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_title ((GtkWindow *) config_window, _("Interface Preferences"));
    gtk_window_set_resizable ((GtkWindow *) config_window, FALSE);
    gtk_container_set_border_width ((GtkContainer *) config_window, 6);

    gtk_container_add ((GtkContainer *) config_window, appearance_page_vbox);
    gtk_widget_show_all (config_window);
}

void skins_configure_cleanup (void)
{
    if (config_window)
        gtk_widget_destroy (config_window);
}
