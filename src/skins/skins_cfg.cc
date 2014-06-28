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

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/preferences.h>
#include <libaudcore/audstrings.h>

#include "dnd.h"
#include "skins_cfg.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_main_evlisteners.h"
#include "ui_playlist.h"
#include "ui_skin.h"
#include "ui_skinned_playlist.h"
#include "ui_skinned_textbox.h"
#include "ui_skinselector.h"
#include "ui_vis.h"
#include "util.h"

static const char * const skins_defaults[] = {
 /* general */
 "autoscroll_songname", "TRUE",
 "mainwin_font", "Sans Bold 9",
 "mainwin_use_bitmapfont", "TRUE",
 "playlist_font", "Sans Bold 8",
 "show_remaining_time", "FALSE",
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
 nullptr};

skins_cfg_t config;

static GtkWidget * skin_view;

typedef struct skins_cfg_boolent_t {
    const char * name;
    bool * ptr;
} skins_cfg_boolent;

static const skins_cfg_boolent skins_boolents[] = {
 /* general */
 {"autoscroll_songname", & config.autoscroll},
 {"mainwin_use_bitmapfont", & config.mainwin_use_bitmapfont},
 {"twoway_scroll", & config.twoway_scroll},

 /* visualizer */
 {"analyzer_peaks", & config.analyzer_peaks}};

typedef struct skins_cfg_nument_t {
    const char * name;
    int * ptr;
} skins_cfg_nument;

static const skins_cfg_nument skins_numents[] = {
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

void skins_cfg_load (void)
{
    aud_config_set_defaults ("skins", skins_defaults);

    for (int i = 0; i < ARRAY_LEN (skins_boolents); i ++)
        * skins_boolents[i].ptr = aud_get_bool ("skins", skins_boolents[i].name);

    for (int i = 0; i < ARRAY_LEN (skins_numents); i ++)
        * skins_numents[i].ptr = aud_get_int ("skins", skins_numents[i].name);
}

void skins_cfg_save (void)
{
    for (int i = 0; i < ARRAY_LEN (skins_boolents); i ++)
        aud_set_bool ("skins", skins_boolents[i].name, * skins_boolents[i].ptr);

    for (int i = 0; i < ARRAY_LEN (skins_numents); i ++)
        aud_set_int ("skins", skins_numents[i].name, * skins_numents[i].ptr);
}

static void
mainwin_font_set_cb()
{
    String font = aud_get_str ("skins", "mainwin_font");
    textbox_set_font (mainwin_info, config.mainwin_use_bitmapfont ? nullptr : (const char *) font);
}

static void
playlist_font_set_cb()
{
    String font = aud_get_str ("skins", "playlist_font");
    ui_skinned_playlist_set_font (playlistwin_list, font);
}

static void autoscroll_set_cb (void)
{
    textbox_set_scroll (mainwin_info, config.autoscroll);
    textbox_set_scroll (playlistwin_sinfo, config.autoscroll);
}

static void vis_reset_cb (void)
{
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
    start_stop_visual (FALSE);
}

static const PreferencesWidget font_table_elements[] = {
    WidgetFonts (N_("Player:"),
        WidgetString ("skins", "mainwin_font", mainwin_font_set_cb),
        {N_("Select main player window font:")}),
    WidgetFonts (N_("Playlist:"),
        WidgetString ("skins", "playlist_font", playlist_font_set_cb),
        {N_("Select playlist font:")})
};

static void * create_skin_view (void);

static const PreferencesWidget skins_widgets_general[] = {
    WidgetLabel (N_("<b>Skin</b>")),
    WidgetCustom (create_skin_view),
    WidgetLabel (N_("<b>Fonts</b>")),
    WidgetTable ({font_table_elements, ARRAY_LEN (font_table_elements)},
        WIDGET_CHILD),
    WidgetCheck (N_("Use bitmap fonts (supports ASCII only)"),
        WidgetBool (config.mainwin_use_bitmapfont, mainwin_font_set_cb)),
    WidgetCheck (N_("Scroll song title"),
        WidgetBool (config.autoscroll, autoscroll_set_cb)),
    WidgetCheck (N_("Scroll song title in both directions"),
        WidgetBool (config.twoway_scroll, autoscroll_set_cb))
};

static ComboBoxElements vis_mode_elements[] = {
    {GINT_TO_POINTER (VIS_ANALYZER), N_("Analyzer")},
    {GINT_TO_POINTER (VIS_SCOPE), N_("Scope")},
    {GINT_TO_POINTER (VIS_VOICEPRINT), N_("Voiceprint / VU meter")},
    {GINT_TO_POINTER (VIS_OFF), N_("Off")}
};

static ComboBoxElements analyzer_mode_elements[] = {
    {GINT_TO_POINTER (ANALYZER_NORMAL), N_("Normal")},
    {GINT_TO_POINTER (ANALYZER_FIRE), N_("Fire")},
    {GINT_TO_POINTER (ANALYZER_VLINES), N_("Vertical lines")}
};

static ComboBoxElements analyzer_type_elements[] = {
    {GINT_TO_POINTER (ANALYZER_LINES), N_("Lines")},
    {GINT_TO_POINTER (ANALYZER_BARS), N_("Bars")}
};

static ComboBoxElements falloff_elements[] = {
    {GINT_TO_POINTER (FALLOFF_SLOWEST), N_("Slowest")},
    {GINT_TO_POINTER (FALLOFF_SLOW), N_("Slow")},
    {GINT_TO_POINTER (FALLOFF_MEDIUM), N_("Medium")},
    {GINT_TO_POINTER (FALLOFF_FAST), N_("Fast")},
    {GINT_TO_POINTER (FALLOFF_FASTEST), N_("Fastest")}
};

static ComboBoxElements scope_mode_elements[] = {
    {GINT_TO_POINTER (SCOPE_DOT), N_("Dots")},
    {GINT_TO_POINTER (SCOPE_LINE), N_("Line")},
    {GINT_TO_POINTER (SCOPE_SOLID), N_("Solid")}
};

static ComboBoxElements voiceprint_mode_elements[] = {
    {GINT_TO_POINTER (VOICEPRINT_NORMAL), N_("Normal")},
    {GINT_TO_POINTER (VOICEPRINT_FIRE), N_("Fire")},
    {GINT_TO_POINTER (VOICEPRINT_ICE), N_("Ice")}
};

static ComboBoxElements vu_mode_elements[] = {
    {GINT_TO_POINTER (VU_NORMAL), N_("Normal")},
    {GINT_TO_POINTER (VU_SMOOTH), N_("Smooth")}
};

static const PreferencesWidget skins_widgets_vis[] = {
    WidgetLabel (N_("<b>Type</b>")),
    WidgetCombo (N_("Visualization type:"),
        WidgetInt (config.vis_type, vis_reset_cb),
        {vis_mode_elements, ARRAY_LEN (vis_mode_elements)}),
    WidgetLabel (N_("<b>Analyzer</b>")),
    WidgetCheck (N_("Show peaks"),
        WidgetBool (config.analyzer_peaks, vis_reset_cb)),
    WidgetCombo (N_("Coloring:"),
        WidgetInt (config.analyzer_mode, vis_reset_cb),
        {analyzer_mode_elements, ARRAY_LEN (analyzer_mode_elements)}),
    WidgetCombo (N_("Style:"),
        WidgetInt (config.analyzer_type, vis_reset_cb),
        {analyzer_type_elements, ARRAY_LEN (analyzer_type_elements)}),
    WidgetCombo (N_("Falloff:"),
        WidgetInt (config.analyzer_falloff, vis_reset_cb),
        {falloff_elements, ARRAY_LEN (falloff_elements)}),
    WidgetCombo (N_("Peak falloff:"),
        WidgetInt (config.peaks_falloff, vis_reset_cb),
        {falloff_elements, ARRAY_LEN (falloff_elements)}),
    WidgetLabel (N_("<b>Miscellaneous</b>")),
    WidgetCombo (N_("Scope Style:"),
        WidgetInt (config.scope_mode, vis_reset_cb),
        {scope_mode_elements, ARRAY_LEN (scope_mode_elements)}),
    WidgetCombo (N_("Voiceprint Coloring:"),
        WidgetInt (config.voiceprint_mode, vis_reset_cb),
        {voiceprint_mode_elements, ARRAY_LEN (voiceprint_mode_elements)}),
    WidgetCombo (N_("VU Meter Style:"),
        WidgetInt (config.vu_mode, vis_reset_cb),
        {vu_mode_elements, ARRAY_LEN (vu_mode_elements)})
};

static const NotebookTab skins_notebook_tabs[] = {
    {N_("General"), skins_widgets_general, ARRAY_LEN (skins_widgets_general)},
    {N_("Visualization"), skins_widgets_vis, ARRAY_LEN (skins_widgets_vis)}
};

static const PreferencesWidget skins_widgets[] = {
    WidgetNotebook ({skins_notebook_tabs, ARRAY_LEN (skins_notebook_tabs)})
};

const PluginPreferences skins_prefs = {
    skins_widgets,
    ARRAY_LEN (skins_widgets)
};

void
on_skin_view_drag_data_received(GtkWidget * widget,
                                GdkDragContext * context,
                                int x, int y,
                                GtkSelectionData * selection_data,
                                unsigned info, unsigned time,
                                void * user_data)
{
    const char * data = (const char *) gtk_selection_data_get_data (selection_data);
    g_return_if_fail (data);

    const char * end = strchr (data, '\r');
    if (! end) end = strchr (data, '\n');
    if (! end) end = data + strlen (data);

    StringBuf path = str_copy (data, end - data);

    if (strstr (path, "://"))
    {
        StringBuf path2 = uri_to_filename (path);
        if (path2)
            path.steal (std::move (path2));
    }

    if (file_is_archive(path))
    {
        if (! active_skin_load (path))
            return;

        skin_install_skin(path);

        if (skin_view)
            skin_view_update ((GtkTreeView *) skin_view);
    }
}

static void * create_skin_view (void)
{
    GtkWidget * scrolled = gtk_scrolled_window_new (nullptr, nullptr);
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
     (GCallback) on_skin_view_drag_data_received, nullptr);
    g_signal_connect (skin_view, "destroy", (GCallback) gtk_widget_destroyed, & skin_view);

    return scrolled;
}
