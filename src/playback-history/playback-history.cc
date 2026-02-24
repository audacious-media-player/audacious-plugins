/*
 * playback-history.cc
 *
 * Copyright (C) 2023-2024 Igor Kushnir <igorkuo@gmail.com>
 * Copyright (C) 2025 Jim Turner <turnerjw784@yahoo.com> (GTK version)
 * Copyright (C) 2026 Thomas Lange <thomas-lange2@gmx.de> (GTK version)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cstring>
#include <utility>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#define AUD_GLIB_INTEGRATION
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/index.h>
#include <libaudcore/objects.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>
#include <libaudgui/gtk-compat.h>
#include <libaudgui/list.h>

#include "history-entry.h"
#include "preferences.h"

class PlaybackHistory : public GeneralPlugin
{
private:
    static constexpr const char * about = aboutText;
    static const PluginPreferences prefs;

public:
    static constexpr PluginInfo info = {N_("Playback History"), PACKAGE,
                                        about, &prefs, PluginGLibOnly};

    constexpr PlaybackHistory() : GeneralPlugin(info, false) {}

    bool init() override;
    void * get_gtk_widget() override;
    int take_message(const char * code, const void *, int) override;
};

EXPORT PlaybackHistory aud_plugin_instance;

static GtkWidget * m_treeview = nullptr;
static Index<HistoryEntry> m_entries;
static int m_playing_position = -1;

static int pos_to_row(int position)
{
    return m_entries.len() - 1 - position;
}

static int row_to_pos(int row)
{
    return pos_to_row(row); // self-inverse function
}

static void get_value(void * user, int row, int column, GValue * value)
{
    g_return_if_fail(row >= 0 && row < m_entries.len());
    g_return_if_fail(column == 0);

    int pos = row_to_pos(row);
    HistoryEntry & entry = m_entries[pos];
    g_value_set_string(value, entry.text());
}

static bool get_selected(void * user, int row)
{
    return false; // true causes new entries to be highlighted ("selected")
}

static void set_selected(void * user, int row, bool selected)
{
    g_return_if_fail(row >= 0 && row < m_entries.len());

    if (selected)
        m_entries[row_to_pos(row)].makeCurrent();
}

static void select_all(void * user, bool selected)
{
    // Required for set_selected() to work
}

static void activate_row(void * user, int row)
{
    g_return_if_fail(row >= 0 && row < m_entries.len());

    int pos = row_to_pos(row);
    if (!m_entries[pos].play())
        return;

    // Update m_playing_position here to prevent the imminent playback_started()
    // invocation from appending a copy of the activated entry to m_entries.
    // This does not prevent appending a different-type counterpart entry if the
    // type of the activated entry does not match the currently configured
    // History Item Granularity. Such a scenario is uncommon (happens only when
    // the user switches between the History modes), and so is not specially
    // handled or optimized for.
    if (m_playing_position != pos)
    {
        m_playing_position = pos;
        int highlight_row = pos_to_row(m_playing_position);
        audgui_list_set_highlight(m_treeview, highlight_row);
        audgui_list_set_focus(m_treeview, highlight_row);
    }
}

static void remove_selected_rows()
{
    GtkTreeModel * model;
    GtkTreeView * treeview = (GtkTreeView *)m_treeview;
    GtkTreeSelection * selection = gtk_tree_view_get_selection(treeview);
    GList * rows = gtk_tree_selection_get_selected_rows(selection, &model);

    for (GList * l = g_list_last(rows); l; l = l->prev)
    {
        GtkTreeIter iter;
        GtkTreePath * path = (GtkTreePath *)l->data;

        if (!gtk_tree_model_get_iter(model, &iter, path))
            continue;

        int row = gtk_tree_path_get_indices(path)[0];
        int pos = row_to_pos(row);

        if (pos == m_playing_position)
            m_playing_position = -1;
        else if (m_playing_position > pos)
            m_playing_position--;

        audgui_list_delete_rows(m_treeview, row, 1);
        m_entries.remove(pos, 1);
    }

    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);

    int n_entries = m_entries.len();
    if (m_playing_position >= n_entries)
        m_playing_position = n_entries - 1;

    if (m_playing_position >= 0)
    {
        int highlight_row = pos_to_row(m_playing_position);
        audgui_list_set_highlight(m_treeview, highlight_row);
        audgui_list_set_focus(m_treeview, highlight_row);
    }
}

static void playback_started()
{
    HistoryEntry entry;
    if (!entry.assignPlayingEntry())
        return;

    int n_entries = m_entries.len();
    entry.debugPrint("Started playing ");
    AUDDBG("playing position=%d, entry count=%d\n",
           m_playing_position, n_entries);

    if (m_playing_position >= 0 && m_playing_position < n_entries)
    {
        HistoryEntry & prev_playing_entry = m_entries[m_playing_position];
        if (!entry.shouldAppendEntry(prev_playing_entry))
            return;
    }

    m_playing_position = n_entries;
    m_entries.append(std::move(entry));

    // The last played entry appears at the top of the view.
    // Therefore, the new entry is inserted at row 0.
    audgui_list_insert_rows(m_treeview, 0, 1);
    audgui_list_set_highlight(m_treeview, 0);
    audgui_list_set_focus(m_treeview, 0);
}

static char * escape_text_safe(const char * text)
{
    return text ? g_markup_escape_text(text, -1) : g_strdup(text);
}

static gboolean tooltip_cb(GtkWidget * widget, int x, int y,
                           gboolean keyboard_mode, GtkTooltip * tooltip,
                           void * data)
{
    int row = audgui_list_row_at_point(widget, x, y);
    if (row < 0)
        return false;

    int pos = row_to_pos(row);
    HistoryEntry & entry = m_entries[pos];

    CharPtr type(escape_text_safe(entry.translatedTextDesignation()));
    CharPtr title(escape_text_safe(entry.text()));
    CharPtr playlist(escape_text_safe(entry.playlistTitle()));

    StringBuf markup = str_printf(
        _("<b>%s:</b> %s\n"
          "<b>Playlist:</b> %s\n"
          "<b>Entry Number:</b> %d"),
        static_cast<const char *>(type), static_cast<const char *>(title),
        static_cast<const char *>(playlist), entry.entryNumber());

    gtk_tooltip_set_markup(tooltip, markup);
    return true;
}

static gboolean keypress_cb(GtkWidget * widget, GdkEventKey * event)
{
    if ((event->state & GDK_CONTROL_MASK) == 0 &&
        event->keyval == GDK_KEY_Delete)
    {
        remove_selected_rows();
        return true;
    }

    return false;
}

static const AudguiListCallbacks callbacks = {
    get_value,
    get_selected,
    set_selected,
    select_all,
    activate_row
};

static GtkWidget * build_widget()
{
    m_treeview = audgui_list_new(&callbacks, nullptr, 0);
    audgui_list_add_column(m_treeview, nullptr, 0, G_TYPE_STRING, -1);
    gtk_widget_set_has_tooltip(m_treeview, true);
    gtk_tree_view_set_headers_visible((GtkTreeView *)m_treeview, false);
    g_signal_connect(m_treeview, "query-tooltip", (GCallback)tooltip_cb, nullptr);
    g_signal_connect(m_treeview, "key-press-event", (GCallback)keypress_cb, nullptr);

    GtkWidget * scrollview = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type((GtkScrolledWindow *)scrollview, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy((GtkScrolledWindow *)scrollview,
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add((GtkContainer *)scrollview, m_treeview);

    GtkWidget * vbox = audgui_vbox_new(6);
    gtk_box_pack_start((GtkBox *)vbox, scrollview, true, true, 0);
    return vbox;
}

static void destroy_cb()
{
    hook_dissociate("playback ready", (HookFunction)playback_started);
    m_entries.clear();
    m_playing_position = -1;
    m_treeview = nullptr;
}

bool PlaybackHistory::init()
{
    aud_config_set_defaults(configSection, defaults);
    return true;
}

void * PlaybackHistory::get_gtk_widget()
{
    GtkWidget * vbox = build_widget();
    hook_associate("playback ready", (HookFunction)playback_started, nullptr);
    g_signal_connect(vbox, "destroy", (GCallback)destroy_cb, nullptr);
    return vbox;
}

int PlaybackHistory::take_message(const char * code, const void *, int)
{
    if (!strcmp(code, "grab focus") && m_treeview)
    {
        gtk_widget_grab_focus(m_treeview);
        return 0;
    }

    return -1;
}

const PluginPreferences PlaybackHistory::prefs = {{widgets}};
