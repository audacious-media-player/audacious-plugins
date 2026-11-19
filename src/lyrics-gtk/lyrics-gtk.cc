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

#include <algorithm> // For std::sort
#include <clocale>
#include <regex>   // For std::regex and std::smatch
#include <sstream> // For std::istringstream
#include <vector>

struct TimedLyricLine
{
    int timestamp_ms; // Timestamp in milliseconds
    String text;      // Lyric text at this timestamp
};

std::vector<TimedLyricLine>
    timed_lyrics; // Stores parsed lyrics with timestamps

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

    bool init () override;
    void * get_gtk_widget () override;
};

EXPORT LyricsGtk aud_plugin_instance;

const PluginPreferences LyricsGtk::prefs = {{widgets}};

FileProvider file_provider;
LrcLibProvider lrclib_provider;
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

    // Parse the lyrics and populate timed_lyrics
    timed_lyrics.clear ();

    // Add a dummy timestamp for the first line (at -1ms to ensure it's always
    // before the actual lyrics)
    TimedLyricLine title_line;
    title_line.timestamp_ms = -1;
    title_line.text = String (title);
    timed_lyrics.push_back (title_line);

    std::istringstream iss (lyrics);
    std::string line;

    int global_offset = 0;
    std::regex time_re (R"(\[\s*(\d+)\s*:\s*(\d+(?:\.\d+)?)\s*\])");
    std::regex offset_re (R"(\[\s*offset\s*:\s*([+-]?\d+)\s*\])",
                         std::regex_constants::icase);

    while (std::getline (iss, line))
    {
        // Sanitize the line: remove leading/trailing spaces and carriage return
        line.erase (0, line.find_first_not_of (" \t\r")); // Remove leading whitespace and \r
        line.erase (line.find_last_not_of (" \t\r") + 1); // Remove trailing whitespace and \r

        if (line.empty ())
            continue;

        // 1. Check for the global offset tag
        std::smatch offset_match;
        if (std::regex_search (line, offset_match, offset_re))
        {
            global_offset = std::stoi (offset_match[1].str());
            continue;
        }

        // 2. Extract MULTIPLE time tags from a single line
        std::sregex_iterator it (line.begin (), line.end (), time_re);
        std::sregex_iterator end;

        std::vector <int> timestamps;
        size_t last_pos = 0;

        for (; it != end; ++it)
        {
            int minutes = std::stoi (it->str (1));
            float seconds = std::stof (it->str (2));
            int timestamp_ms =
                static_cast <int> ((minutes * 60 + seconds) * 1000);

            timestamps.push_back (timestamp_ms);
            last_pos = it->position () + it->length ();
        }

        // 3. If timestamps were found, attach the remaining text to all of them
        if (!timestamps.empty ())
        {
            std::string text = line.substr (last_pos);
            text.erase (0, text.find_first_not_of (" \t")); // Trim leading spaces

            for (int ts : timestamps)
            {
                TimedLyricLine timed_line;
                timed_line.timestamp_ms = ts;
                timed_line.text = String (text.c_str ());

                timed_lyrics.push_back (timed_line);
            }
        }
    }

    // 4. Apply offsets and sort chronologically (skipping the dummy title line
    // at index 0)
    if (timed_lyrics.size () > 1)
    {
        for (size_t i = 1; i < timed_lyrics.size (); ++i)
        {
            timed_lyrics[i].timestamp_ms -= global_offset;
        }

        std::sort (timed_lyrics.begin () + 1, timed_lyrics.end (),
                  [](const TimedLyricLine & a, const TimedLyricLine & b) {
                      return a.timestamp_ms < b.timestamp_ms;
                  });

        // Ensure the dummy title line stays chronologically before the first
        // actual lyric
        if (timed_lyrics[1].timestamp_ms <= timed_lyrics[0].timestamp_ms)
        {
            timed_lyrics[0].timestamp_ms = timed_lyrics[1].timestamp_ms - 1000;
        }
    }

    // gtk_text_buffer_get_start_iter (textbuffer, & iter);
    // gtk_text_view_scroll_to_iter (textview, & iter, 0, true, 0, 0);
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
#ifdef USE_GTK3
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
    gtk_text_buffer_create_tag (textbuffer, "highlight", "foreground", "yellow", nullptr);
    gtk_text_buffer_create_tag (textbuffer, "weight_bold", "weight", PANGO_WEIGHT_BOLD, nullptr);
    gtk_text_buffer_create_tag (textbuffer, "scale_large", "scale", PANGO_SCALE_LARGE, nullptr);
    gtk_text_buffer_create_tag (textbuffer, "style_italic", "style", PANGO_STYLE_ITALIC, nullptr);

    GtkWidget * hbox = audgui_hbox_new (6);
    gtk_box_pack_start ((GtkBox *) vbox, hbox, false, false, 0);

    return vbox;
}

void highlight_lyrics (int current_time_ms)
{
    if (!textbuffer)
        return;

    // Check if lyrics synchronization is enabled
    if (!aud_get_bool ("lyricwiki", "sync_lyrics"))
        return;

    // Clear the text buffer
    gtk_text_buffer_set_text (textbuffer, "", -1);

    // Find the 4 lines closest to the current timestamp (current + 3 neighbors)
    std::vector <TimedLyricLine> lines_to_display;

    // Store up to 4 lines starting from the current one
    for (size_t i = 0; i < timed_lyrics.size (); ++i)
    {

        if (timed_lyrics[i].timestamp_ms >= current_time_ms)
        {
            // Ensure we have the current line and its neighbors (3 more lines)
            size_t start_index = (i > 1) ? i - 2 : 0; // Start 2 lines before the current line
            size_t end_index = std::min (i + 2, timed_lyrics.size () - 1); // Limit to 3 more lines after

            // Ensure no more than 4 lines are selected
            size_t line_count = 0;

            for (size_t j = start_index; j <= end_index && line_count < 4; ++j)
            {
                lines_to_display.push_back (timed_lyrics[j]);
                line_count++;
            }
            break;
        }
    }

    // Retrieve the tag table
    GtkTextTagTable * tag_table = gtk_text_buffer_get_tag_table (textbuffer);

    // Check if the enlarge tag exists, and create it if not
    GtkTextTag * enlarge_tag =
        gtk_text_tag_table_lookup (tag_table, "enlarge_tag");
    if (!enlarge_tag)
    {
        enlarge_tag = gtk_text_tag_new ("enlarge_tag");
        g_object_set (enlarge_tag, "scale", 1.5, NULL); // Enlarge by 1.5 times (adjust as needed)
        gtk_text_tag_table_add (tag_table, enlarge_tag);
    }

    // Insert the selected lines into the text buffer
    GtkTextIter iter;
    gtk_text_buffer_get_start_iter (textbuffer, &iter);

    for (size_t i = 0; i < lines_to_display.size (); ++i)
    {
        const TimedLyricLine & line = lines_to_display[i];
        std::string text_with_newline = std::string (line.text);

        // Special handling for the title line (dummy timestamp)
        if (line.timestamp_ms < 0 && i == 0 &&
            text_with_newline ==
                static_cast <const char *> (timed_lyrics[0].text))
        {
            gtk_text_buffer_insert_with_tags_by_name (
                textbuffer, &iter, text_with_newline.c_str (), -1, "weight_bold",
                "scale_large", nullptr);
        }
        else if (i == 1 && line.timestamp_ms >= 0)
        { // Current line (but not title)
            gtk_text_buffer_insert_with_tags_by_name (textbuffer, &iter,
                                                     text_with_newline.c_str (),
                                                     -1, "highlight", nullptr);
        }
        else
        {
            gtk_text_buffer_insert (textbuffer, &iter, text_with_newline.c_str (), -1);
        }

        gtk_text_buffer_insert (textbuffer, &iter, "\n", -1);
    }

    // After inserting lines, force scroll to the last line
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter (textbuffer, &end_iter);
    gtk_text_view_scroll_to_iter (textview, &end_iter, 0, TRUE, 0, 0);
}

gboolean update_lyrics_display (gpointer data)
{
    int current_time_ms =
        aud_drct_get_time (); // Get current time from player in ms
    highlight_lyrics (current_time_ms);

    return G_SOURCE_CONTINUE; // Continue calling this function
}

void * LyricsGtk::get_gtk_widget ()
{
    GtkWidget * vbox = build_widget ();

    hook_associate ("tuple change", (HookFunction) lyrics_playback_began, nullptr);
    hook_associate ("playback ready", (HookFunction) lyrics_playback_began, nullptr);

    if (aud_drct_get_ready ())
        lyrics_playback_began ();

    g_signal_connect (vbox, "destroy", (GCallback) destroy_cb, nullptr);
    g_timeout_add (100, update_lyrics_display, nullptr);
    return vbox;
}
