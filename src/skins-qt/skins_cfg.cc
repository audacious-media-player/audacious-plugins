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

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/preferences.h>
#include <libaudcore/audstrings.h>

#include "dnd.h"
#include "skins_cfg.h"
#include "main.h"
#include "vis-callbacks.h"
#include "playlist.h"
#include "skin.h"
#include "playlist-widget.h"
#include "textbox.h"
#include "skinselector.h"
#include "vis.h"
#include "util.h"
#include "view.h"

static const char * const skins_defaults[] = {
    /* general */
    "autoscroll_songname", "TRUE",
    "mainwin_font", "Sans Bold 9",
    "mainwin_use_bitmapfont", "TRUE",
    "playlist_font", "Sans Bold 8",
    "record", "FALSE",
    "show_remaining_time", "FALSE",
    "twoway_scroll", "FALSE",

    /* visualizer */
    "analyzer_falloff", aud::numeric_string<FALLOFF_FAST>::str,
    "analyzer_mode", aud::numeric_string<ANALYZER_NORMAL>::str,
    "analyzer_peaks", "TRUE",
    "analyzer_type", aud::numeric_string<ANALYZER_BARS>::str,
    "peaks_falloff", aud::numeric_string<FALLOFF_SLOW>::str,
    "scope_mode", aud::numeric_string<SCOPE_DOT>::str,
    "vis_type", aud::numeric_string<VIS_ANALYZER>::str,
    "voiceprint_mode", aud::numeric_string<VOICEPRINT_NORMAL>::str,
    "vu_mode", aud::numeric_string<VU_SMOOTH>::str,

    /* windows */
    "always_on_top", "FALSE",
    "double_size", "FALSE",
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

    nullptr
};

skins_cfg_t config;

static const struct skins_cfg_boolent_t {
    const char * name;
    bool * ptr;
} skins_boolents[] =
{
    /* general */
    {"autoscroll_songname", & config.autoscroll},
    {"mainwin_use_bitmapfont", & config.mainwin_use_bitmapfont},
    {"twoway_scroll", & config.twoway_scroll},

    /* visualizer */
    {"analyzer_peaks", & config.analyzer_peaks}
};

static const struct {
    const char * name;
    int * ptr;
} skins_numents[] =
{
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
    {"playlist_height", & config.playlist_height}
};

static String selected_skin;
static Index<ComboItem> skin_combo;

void skins_cfg_load ()
{
    aud_config_set_defaults ("skins", skins_defaults);

    for (auto & boolent : skins_boolents)
        * boolent.ptr = aud_get_bool ("skins", boolent.name);

    for (auto & nument : skins_numents)
        * nument.ptr = aud_get_int ("skins", nument.name);
}

void skins_cfg_save ()
{
    for (auto & boolent : skins_boolents)
        aud_set_bool ("skins", boolent.name, * boolent.ptr);

    for (auto & nument : skins_numents)
        aud_set_int ("skins", nument.name, * nument.ptr);
}

static ArrayRef<ComboItem> skin_combo_fill ()
{
    selected_skin = aud_get_str ("skins", "skin");

    skin_combo.clear ();
    skinlist_update ();

    for (auto & node : skinlist)
        skin_combo.append (node.name, node.path);

    return {skin_combo.begin (), skin_combo.len ()};
}

static void skin_select_cb ()
{
    if (skin_load (selected_skin))
        view_apply_skin ();
}

static void mainwin_font_set_cb ()
{
    if (! config.mainwin_use_bitmapfont)
        mainwin_info->set_font (aud_get_str ("skins", "mainwin_font"));
    else
        mainwin_info->set_font (nullptr);
}

static void playlist_font_set_cb ()
{
    playlistwin_list->set_font (aud_get_str ("skins", "playlist_font"));
}

static void autoscroll_set_cb ()
{
    if (! aud_get_bool ("skins", "mainwin_shaded"))
        mainwin_info->set_scroll (config.autoscroll);
    if (aud_get_bool ("skins", "playlist_shaded"))
        playlistwin_sinfo->set_scroll (config.autoscroll);
}

static void vis_reset_cb ()
{
    mainwin_vis->clear ();
    mainwin_svis->clear ();
    start_stop_visual (false);
}

static const PreferencesWidget font_table_elements[] = {
    WidgetFonts (N_("Player:"),
        WidgetString ("skins", "mainwin_font", mainwin_font_set_cb),
        {N_("Select main player window font:")}),
    WidgetFonts (N_("Playlist:"),
        WidgetString ("skins", "playlist_font", playlist_font_set_cb),
        {N_("Select playlist font:")})
};

static const PreferencesWidget skins_widgets_general[] = {
    WidgetLabel (N_("<b>Skin</b>")),
    WidgetCombo (nullptr,
        WidgetString (selected_skin, skin_select_cb),
        {nullptr, skin_combo_fill}),
    WidgetLabel (N_("<b>Fonts</b>")),
    WidgetTable ({{font_table_elements}}),
    WidgetCheck (N_("Use bitmap fonts (supports ASCII only)"),
        WidgetBool (config.mainwin_use_bitmapfont, mainwin_font_set_cb)),
    WidgetCheck (N_("Scroll song title"),
        WidgetBool (config.autoscroll, autoscroll_set_cb)),
    WidgetCheck (N_("Scroll song title in both directions"),
        WidgetBool (config.twoway_scroll, autoscroll_set_cb))
};

static ComboItem vis_mode_elements[] = {
    ComboItem (N_("Analyzer"), VIS_ANALYZER),
    ComboItem (N_("Scope"), VIS_SCOPE),
    ComboItem (N_("Voiceprint / VU meter"), VIS_VOICEPRINT),
    ComboItem (N_("Off"), VIS_OFF)
};

static ComboItem analyzer_mode_elements[] = {
    ComboItem (N_("Normal"), ANALYZER_NORMAL),
    ComboItem (N_("Fire"), ANALYZER_FIRE),
    ComboItem (N_("Vertical lines"), ANALYZER_VLINES)
};

static ComboItem analyzer_type_elements[] = {
    ComboItem (N_("Lines"), ANALYZER_LINES),
    ComboItem (N_("Bars"), ANALYZER_BARS)
};

static ComboItem falloff_elements[] = {
    ComboItem (N_("Slowest"), FALLOFF_SLOWEST),
    ComboItem (N_("Slow"), FALLOFF_SLOW),
    ComboItem (N_("Medium"), FALLOFF_MEDIUM),
    ComboItem (N_("Fast"), FALLOFF_FAST),
    ComboItem (N_("Fastest"), FALLOFF_FASTEST)
};

static ComboItem scope_mode_elements[] = {
    ComboItem (N_("Dots"), SCOPE_DOT),
    ComboItem (N_("Line"), SCOPE_LINE),
    ComboItem (N_("Solid"), SCOPE_SOLID)
};

static ComboItem voiceprint_mode_elements[] = {
    ComboItem (N_("Normal"), VOICEPRINT_NORMAL),
    ComboItem (N_("Fire"), VOICEPRINT_FIRE),
    ComboItem (N_("Ice"), VOICEPRINT_ICE)
};

static ComboItem vu_mode_elements[] = {
    ComboItem (N_("Normal"), VU_NORMAL),
    ComboItem (N_("Smooth"), VU_SMOOTH)
};

static const PreferencesWidget analyzer_table[] = {
    WidgetCombo (N_("Coloring:"),
        WidgetInt (config.analyzer_mode, vis_reset_cb),
        {{analyzer_mode_elements}}),
    WidgetCombo (N_("Style:"),
        WidgetInt (config.analyzer_type, vis_reset_cb),
        {{analyzer_type_elements}}),
    WidgetCombo (N_("Falloff:"),
        WidgetInt (config.analyzer_falloff, vis_reset_cb),
        {{falloff_elements}}),
    WidgetCombo (N_("Peak falloff:"),
        WidgetInt (config.peaks_falloff, vis_reset_cb),
        {{falloff_elements}})
};

static const PreferencesWidget misc_table[] = {
    WidgetCombo (N_("Scope Style:"),
        WidgetInt (config.scope_mode, vis_reset_cb),
        {{scope_mode_elements}}),
    WidgetCombo (N_("Voiceprint Coloring:"),
        WidgetInt (config.voiceprint_mode, vis_reset_cb),
        {{voiceprint_mode_elements}}),
    WidgetCombo (N_("VU Meter Style:"),
        WidgetInt (config.vu_mode, vis_reset_cb),
        {{vu_mode_elements}})
};

static const PreferencesWidget skins_widgets_vis[] = {
    WidgetLabel (N_("<b>Type</b>")),
    WidgetCombo (N_("Visualization type:"),
        WidgetInt (config.vis_type, vis_reset_cb),
        {{vis_mode_elements}}),
    WidgetLabel (N_("<b>Analyzer</b>")),
    WidgetCheck (N_("Show peaks"),
        WidgetBool (config.analyzer_peaks, vis_reset_cb)),
    WidgetTable ({{analyzer_table}}),
    WidgetLabel (N_("<b>Miscellaneous</b>")),
    WidgetTable ({{misc_table}})
};

static const NotebookTab skins_notebook_tabs[] = {
    {N_("General"), {skins_widgets_general}},
    {N_("Visualization"), {skins_widgets_vis}}
};

static const PreferencesWidget skins_widgets[] = {
    WidgetNotebook ({{skins_notebook_tabs}})
};

const PluginPreferences skins_prefs = {{skins_widgets}};

#if 0
void on_skin_view_drag_data_received (GtkWidget * widget, GdkDragContext * context,
 int x, int y, GtkSelectionData * selection_data, unsigned info, unsigned time, void *)
{
    const char * data = (const char *) gtk_selection_data_get_data (selection_data);
    g_return_if_fail (data);

    const char * end = strchr (data, '\r');
    if (! end)
        end = strchr (data, '\n');
    if (! end)
        end = data + strlen (data);

    StringBuf path = str_copy (data, end - data);

    if (strstr (path, "://"))
    {
        StringBuf path2 = uri_to_filename (path);
        if (path2)
            path.steal (std::move (path2));
    }

    if (file_is_archive (path))
    {
        if (! skin_load (path))
            return;

        view_apply_skin ();
        skin_install_skin(path);

        if (skin_view)
            skin_view_update ((GtkTreeView *) skin_view);
    }
}
#endif
