/*
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

#include "lyrics.h"

void ChartLyricsProvider::reset_lyric_metadata ()
{
    m_lyric_id = -1;
    m_lyric_checksum = String ();
    m_lyric_url = String ();
    m_lyrics = String ();
}

String ChartLyricsProvider::match_uri (LyricsState state)
{
    auto artist = str_copy (state.artist);
    artist = str_encode_percent (artist, -1);

    auto title = str_copy (state.title);
    title = str_encode_percent (title, -1);

    return String (str_concat ({m_base_url, "/SearchLyric?artist=", artist, "&song=", title}));
}

bool ChartLyricsProvider::has_match (LyricsState state, xmlNodePtr node)
{
    String lyric_id, checksum, url, artist, title;

    for (xmlNodePtr cur_node = node->xmlChildrenNode; cur_node; cur_node = cur_node->next)
    {
        if (cur_node->type != XML_ELEMENT_NODE)
            continue;

        xmlChar * content = xmlNodeGetContent (cur_node);

        if (xmlStrEqual (cur_node->name, (xmlChar *) "LyricId"))
            lyric_id = String ((const char *) content);
        else if (xmlStrEqual (cur_node->name, (xmlChar *) "LyricChecksum"))
            checksum = String ((const char *) content);
        else if (xmlStrEqual (cur_node->name, (xmlChar *) "SongUrl"))
            url = String ((const char *) content);
        else if (xmlStrEqual (cur_node->name, (xmlChar *) "Artist"))
            artist = String ((const char *) content);
        else if (xmlStrEqual (cur_node->name, (xmlChar *) "Song"))
            title = String ((const char *) content);

        xmlFree (content);
    }

    if (lyric_id && checksum && artist && title) // url is optional
    {
        int id = str_to_int (lyric_id);

        if (id > 0 &&
            ! strcmp_nocase (artist, state.artist) &&
            ! strcmp_nocase (title, state.title))
        {
            m_lyric_id = id;
            m_lyric_checksum = checksum;
            m_lyric_url = url;

            return true;
        }
    }

    return false;
}

bool ChartLyricsProvider::match (LyricsState state)
{
    reset_lyric_metadata ();

    auto handle_result_cb = [=] (const char * uri, const Index<char> & buf) {
        if (! buf.len ())
        {
            update_lyrics_window_error (str_printf (_("Unable to fetch %s"), uri));
            return;
        }

        xmlDocPtr doc = xmlReadMemory (buf.begin (), buf.len (), nullptr, nullptr, 0);
        if (! doc)
        {
            update_lyrics_window_error (str_printf (_("Unable to parse %s"), uri));
            return;
        }

        xmlNodePtr root = xmlDocGetRootElement (doc);

        for (xmlNodePtr cur_node = root->xmlChildrenNode; cur_node; cur_node = cur_node->next)
        {
            if (cur_node->type != XML_ELEMENT_NODE)
                continue;

            if (has_match (state, cur_node))
                break;
        }

        xmlFreeDoc (doc);

        fetch (state);
    };

    vfs_async_file_get_contents (match_uri (state), handle_result_cb);
    update_lyrics_window_message (state, _("Looking for lyrics ..."));

    return true;
}

String ChartLyricsProvider::fetch_uri (LyricsState state)
{
    if (m_lyric_id <= 0 || ! m_lyric_checksum)
        return String ();

    auto id = int_to_str (m_lyric_id);
    auto checksum = str_copy (m_lyric_checksum);
    checksum = str_encode_percent (checksum, -1);

    return String (str_concat ({m_base_url, "/GetLyric?lyricId=", id, "&lyricCheckSum=", checksum}));
}

void ChartLyricsProvider::fetch (LyricsState state)
{
    String _fetch_uri = fetch_uri (state);
    if (! _fetch_uri)
    {
        update_lyrics_window_notfound (state);
        return;
    }

    auto handle_result_cb = [=] (const char * uri, const Index<char> & buf) {
        if (! buf.len ())
        {
            update_lyrics_window_error (str_printf (_("Unable to fetch %s"), uri));
            return;
        }

        xmlDocPtr doc = xmlReadMemory (buf.begin (), buf.len (), nullptr, nullptr, 0);
        if (! doc)
        {
            update_lyrics_window_error (str_printf (_("Unable to parse %s"), uri));
            return;
        }

        xmlNodePtr root = xmlDocGetRootElement (doc);

        for (xmlNodePtr cur_node = root->xmlChildrenNode; cur_node; cur_node = cur_node->next)
        {
            if (cur_node->type == XML_ELEMENT_NODE &&
                xmlStrEqual (cur_node->name, (xmlChar *) "Lyric"))
            {
                xmlChar * content = xmlNodeGetContent (cur_node);
                m_lyrics = String ((const char *) content);
                xmlFree (content);
                break;
            }
        }

        xmlFreeDoc (doc);

        LyricsState new_state = g_state;
        new_state.lyrics = String ();

        if (! m_lyrics || ! m_lyrics[0])
        {
            update_lyrics_window_notfound (new_state);
            return;
        }

        new_state.lyrics = m_lyrics;
        new_state.source = LyricsState::Source::ChartLyrics;

        update_lyrics_window (new_state.title, new_state.artist, new_state.lyrics);
        persist_state (new_state);
    };

    vfs_async_file_get_contents (_fetch_uri, handle_result_cb);
    update_lyrics_window_message (state, _("Looking for lyrics ..."));
}
