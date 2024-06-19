/*
 * Copyright (c) 2010-2019 Ariadne Conill <ariadne@dereferenced.org>
 * Copyright (c) 2024 Thomas Lange <thomas-lange2@gmx.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <json-glib/json-glib.h>

#include <libaudcore/hook.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudgui/gtk-compat.h>

#include "../lyrics-common/lyrics.h"
#include "../lyrics-common/preferences.h"

class LyricsGtk : public GeneralPlugin
{
public:
    static const PluginPreferences prefs;
    static constexpr PluginInfo info = {
        N_("Lyrics"),
        PACKAGE,
        nullptr, // about
        & prefs,
        PluginGLibOnly
    };

    constexpr LyricsGtk () : GeneralPlugin (info, false) {}

    bool init ();
    void * get_gtk_widget ();
};

EXPORT LyricsGtk aud_plugin_instance;

const PluginPreferences LyricsGtk::prefs = {{widgets}};

FileProvider file_provider;
ChartLyricsProvider chart_lyrics_provider;
LyricsOVHProvider lyrics_ovh_provider;

LyricsState g_state;

static GtkTextView * textview;
static GtkTextBuffer * textbuffer;

bool LyricsGtk::init ()
{
    aud_config_set_defaults (CFG_SECTION, defaults);
    return true;
}

void update_lyrics_window (const char * title, const char * artist, const char * lyrics)
{
    GtkTextIter iter;

    if (! textbuffer)
        return;

    gtk_text_buffer_set_text (textbuffer, "", -1);
    gtk_text_buffer_get_start_iter (textbuffer, & iter);
    gtk_text_buffer_insert_with_tags_by_name (textbuffer, & iter, title, -1,
     "weight_bold", "scale_large", nullptr);

    if (artist)
    {
        gtk_text_buffer_insert (textbuffer, & iter, "\n", -1);
        gtk_text_buffer_insert_with_tags_by_name (textbuffer, & iter, artist, -1,
         "style_italic", nullptr);
    }

    gtk_text_buffer_insert (textbuffer, & iter, "\n\n", -1);
    gtk_text_buffer_insert (textbuffer, & iter, lyrics, -1);

    gtk_text_buffer_get_start_iter (textbuffer, & iter);
    gtk_text_view_scroll_to_iter (textview, & iter, 0, true, 0, 0);
}

bool try_parse_json (const Index<char> & buf, const char * key, String & output)
{
    JsonParser * parser = json_parser_new ();

    if (! json_parser_load_from_data (parser, buf.begin (), buf.len (), nullptr))
    {
        g_object_unref (parser);
        return false;
    }

    JsonNode * root = json_parser_get_root (parser);
    JsonReader * reader = json_reader_new (root);

    json_reader_read_member (reader, key);
    output = String (json_reader_get_string_value (reader));
    json_reader_end_member (reader);

    g_object_unref (reader);
    g_object_unref (parser);

    return true;
}

static void destroy_cb ()
{
    g_state.filename = String ();
    g_state.title = String ();
    g_state.artist = String ();
    g_state.lyrics = String ();

    hook_dissociate ("tuple change", (HookFunction) lyrics_playback_began);
    hook_dissociate ("playback ready", (HookFunction) lyrics_playback_began);

    textview = nullptr;
    textbuffer = nullptr;
}

static GtkWidget * append_item_to_menu (GtkWidget * menu, const char * label)
{
    GtkWidget * menu_item = (label != nullptr)
     ? gtk_menu_item_new_with_label (label)
     : gtk_separator_menu_item_new ();

    gtk_menu_shell_append ((GtkMenuShell *) menu, menu_item);
    gtk_widget_show (menu_item);

    return menu_item;
}

static void append_separator_to_menu (GtkWidget * menu)
{
    append_item_to_menu (menu, nullptr);
}

static void edit_lyrics_cb (GtkMenuItem * menu_item, void * data)
{
    const char * edit_uri = (const char *) data;
#if GTK_CHECK_VERSION(3, 22, 0)
    gtk_show_uri_on_window (nullptr, edit_uri, GDK_CURRENT_TIME, nullptr);
#else
    gtk_show_uri (nullptr, edit_uri, GDK_CURRENT_TIME, nullptr);
#endif
}

static void save_locally_cb (GtkMenuItem * menu_item, void * data)
{
    file_provider.save (g_state);
}

static void refresh_cb (GtkMenuItem * menu_item, void * data)
{
    LyricProvider * remote_provider = remote_source ();
    if (remote_provider)
        remote_provider->match (g_state);
}

static void populate_popup_cb (GtkTextView * text_view, GtkWidget * menu, void * data)
{
    if (! g_state.artist || ! g_state.title || ! GTK_IS_MENU (menu))
        return;

    GtkWidget * menu_item;
    append_separator_to_menu (menu);

    if (g_state.lyrics && g_state.source != LyricsState::Source::Local && ! g_state.error)
    {
        LyricProvider * remote_provider = remote_source ();

        if (remote_provider)
        {
            String edit_uri = remote_provider->edit_uri (g_state);

            if (edit_uri && edit_uri[0])
            {
                menu_item = append_item_to_menu (menu, _("Edit Lyrics ..."));
                g_signal_connect_data (menu_item, "activate", (GCallback) edit_lyrics_cb,
                 g_strdup (edit_uri), (GClosureNotify) g_free, (GConnectFlags) 0);
            }
        }

        menu_item = append_item_to_menu (menu, _("Save Locally"));
        g_signal_connect (menu_item, "activate", (GCallback) save_locally_cb, nullptr);
    }

    if (g_state.source == LyricsState::Source::Local || g_state.error)
    {
        menu_item = append_item_to_menu (menu, _("Refresh"));
        g_signal_connect (menu_item, "activate", (GCallback) refresh_cb, nullptr);
    }
}

static GtkWidget * build_widget ()
{
    textview = (GtkTextView *) gtk_text_view_new ();
    gtk_text_view_set_editable (textview, false);
    gtk_text_view_set_cursor_visible (textview, false);
    gtk_text_view_set_left_margin (textview, 4);
    gtk_text_view_set_right_margin (textview, 4);
    gtk_text_view_set_wrap_mode (textview, GTK_WRAP_WORD);
    textbuffer = gtk_text_view_get_buffer (textview);

    GtkWidget * scrollview = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scrollview, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scrollview, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    GtkWidget * vbox = audgui_vbox_new (6);

    g_signal_connect (textview, "populate-popup", (GCallback) populate_popup_cb, nullptr);

    gtk_container_add ((GtkContainer *) scrollview, (GtkWidget *) textview);
    gtk_box_pack_start ((GtkBox *) vbox, scrollview, true, true, 0);

    gtk_widget_show_all (vbox);

    gtk_text_buffer_create_tag (textbuffer, "weight_bold", "weight", PANGO_WEIGHT_BOLD, nullptr);
    gtk_text_buffer_create_tag (textbuffer, "scale_large", "scale", PANGO_SCALE_LARGE, nullptr);
    gtk_text_buffer_create_tag (textbuffer, "style_italic", "style", PANGO_STYLE_ITALIC, nullptr);

    GtkWidget * hbox = audgui_hbox_new (6);
    gtk_box_pack_start ((GtkBox *) vbox, hbox, false, false, 0);

    return vbox;
}

void * LyricsGtk::get_gtk_widget ()
{
    GtkWidget * vbox = build_widget ();

    hook_associate ("tuple change", (HookFunction) lyrics_playback_began, nullptr);
    hook_associate ("playback ready", (HookFunction) lyrics_playback_began, nullptr);

    if (aud_drct_get_ready ())
        lyrics_playback_began ();

    g_signal_connect (vbox, "destroy", (GCallback) destroy_cb, nullptr);

    return vbox;
}
