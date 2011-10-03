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

#include <gtk/gtk.h>

#include <audacious/debug.h>
#include <audacious/gtk-compat.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/preferences.h>
#include <libaudcore/audstrings.h>

#include "config.h"
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
 "twoway_scroll", "TRUE",

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
 "player_visible", "TRUE",
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
 {"player_visible", & config.player_visible},
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

static const skins_cfg_strent skins_strents[] = {
 {"skin", & config.skin},
 {"mainwin_font", & config.mainwin_font},
 {"playlist_font", & config.playlist_font}};

void skins_cfg_free (void)
{
    for (gint i = 0; i < G_N_ELEMENTS (skins_strents); i ++)
    {
        g_free (* skins_strents[i].ptr);
        * skins_strents[i].ptr = NULL;
    }
}

void skins_cfg_load (void)
{
    aud_config_set_defaults ("skins", skins_defaults);

    for (gint i = 0; i < G_N_ELEMENTS (skins_boolents); i ++)
        * skins_boolents[i].ptr = aud_get_bool ("skins", skins_boolents[i].name);

    for (gint i = 0; i < G_N_ELEMENTS (skins_numents); i ++)
        * skins_numents[i].ptr = aud_get_int ("skins", skins_numents[i].name);

    for (gint i = 0; i < G_N_ELEMENTS (skins_strents); i ++)
        * skins_strents[i].ptr = aud_get_string ("skins", skins_strents[i].name);
}

void skins_cfg_save (void)
{
    for (gint i = 0; i < G_N_ELEMENTS (skins_boolents); i ++)
        aud_set_bool ("skins", skins_boolents[i].name, * skins_boolents[i].ptr);

    for (gint i = 0; i < G_N_ELEMENTS (skins_numents); i ++)
        aud_set_int ("skins", skins_numents[i].name, * skins_numents[i].ptr);

    for (gint i = 0; i < G_N_ELEMENTS (skins_strents); i ++)
        aud_set_string ("skins", skins_strents[i].name, * skins_strents[i].ptr);
}

static void
mainwin_font_set_cb()
{
    textbox_set_font (mainwin_info, config.mainwin_use_bitmapfont ? NULL :
     config.mainwin_font);
}

static void
playlist_font_set_cb()
{
    AUDDBG("Attempt to set font \"%s\"\n", config.playlist_font);
    ui_skinned_playlist_set_font (playlistwin_list, config.playlist_font);
}

static void
bitmap_fonts_cb()
{
    textbox_set_font (mainwin_info, config.mainwin_use_bitmapfont ? NULL :
     config.mainwin_font);
}

static PreferencesWidget font_table_elements[] = {
 {WIDGET_FONT_BTN, N_("_Player:"), .cfg_type = VALUE_STRING, .cfg = & config.mainwin_font,
  .callback = mainwin_font_set_cb, .data = {.font_btn = {N_("Select main "
  "player window font:")}}},
 {WIDGET_FONT_BTN, N_("_Playlist:"), .cfg_type = VALUE_STRING, .cfg = & config.playlist_font,
  .callback = playlist_font_set_cb, .data = {.font_btn = {N_("Select playlist "
  "font:")}}}};

static PreferencesWidget appearance_misc_widgets[] = {
    {WIDGET_LABEL, N_("<b>_Fonts</b>"), NULL, NULL, NULL, FALSE},
    {WIDGET_TABLE, .child = TRUE, .data = {.table = {font_table_elements,
     G_N_ELEMENTS (font_table_elements)}}},
    {WIDGET_CHK_BTN, N_("Use bitmap fonts (supports ASCII only)"),
     .cfg_type = VALUE_BOOLEAN, .cfg = & config.mainwin_use_bitmapfont, .callback = bitmap_fonts_cb},
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

    gchar * path = g_strndup (data, end - data);

    if (strstr (path, "://"))
    {
        gchar * path2 = uri_to_filename (path);
        if (path2)
        {
            g_free (path);
            path = path2;
        }
    }

    if (file_is_archive(path)) {
        if (! active_skin_load (path))
            return;
        skin_install_skin(path);
        skin_view_update ((GtkTreeView *) widget);
    }
}

static GtkWidget * config_window = NULL;

void skins_configure (void)
{
    if (config_window)
    {
        gtk_window_present ((GtkWindow *) config_window);
        return;
    }

    GtkWidget *appearance_page_vbox;
    GtkWidget *vbox37;
    GtkWidget *vbox38;
    GtkWidget *hbox12;
    GtkWidget *alignment94;
    GtkWidget *hbox13;
    GtkWidget *label103;
    GtkWidget *alignment95;
    GtkWidget *skin_view_scrolled_window;

    appearance_page_vbox = gtk_vbox_new (FALSE, 0);

    vbox37 = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (appearance_page_vbox), vbox37, TRUE, TRUE, 0);

    vbox38 = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox37), vbox38, FALSE, TRUE, 0);

    hbox12 = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox38), hbox12, TRUE, TRUE, 0);

    alignment94 = gtk_alignment_new (0.5, 0.5, 1, 1);
    gtk_box_pack_start (GTK_BOX (hbox12), alignment94, TRUE, TRUE, 0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment94), 0, 4, 0, 0);

    hbox13 = gtk_hbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (alignment94), hbox13);

    label103 = gtk_label_new_with_mnemonic (_("<b>_Skin</b>"));
    gtk_box_pack_start (GTK_BOX (hbox13), label103, TRUE, TRUE, 0);
    gtk_label_set_use_markup (GTK_LABEL (label103), TRUE);
    gtk_misc_set_alignment (GTK_MISC (label103), 0, 0);

    alignment95 = gtk_alignment_new (0.5, 0.5, 1, 1);
    gtk_box_pack_start (GTK_BOX (vbox38), alignment95, TRUE, TRUE, 0);
    gtk_widget_set_size_request (alignment95, -1, 172);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment95), 0, 0, 12, 0);

    skin_view_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (alignment95), skin_view_scrolled_window);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (skin_view_scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (skin_view_scrolled_window), GTK_SHADOW_IN);

    skin_view = gtk_tree_view_new ();
    skin_view_realize ((GtkTreeView *) skin_view);
    skin_view_update ((GtkTreeView *) skin_view);
    gtk_container_add (GTK_CONTAINER (skin_view_scrolled_window), skin_view);
    gtk_widget_set_size_request (skin_view, -1, 100);

    aud_create_widgets(GTK_BOX(vbox37), appearance_misc_widgets, G_N_ELEMENTS(appearance_misc_widgets));

    g_signal_connect(skin_view, "drag-data-received",
                     G_CALLBACK(on_skin_view_drag_data_received),
                     NULL);
    drag_dest_set(skin_view);

    g_signal_connect(mainwin, "drag-data-received",
                     G_CALLBACK(mainwin_drag_data_received),
                     skin_view);

    GtkWidget * hbox = gtk_hbox_new (FALSE, 6);
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
