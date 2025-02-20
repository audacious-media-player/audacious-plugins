/*
 * Copyright (c) 2025 Thomas Lange <thomas-lange2@gmx.de>
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

bool LrcLibProvider::match (LyricsState state)
{
    fetch (state);
    return true;
}

void LrcLibProvider::fetch (LyricsState state)
{
    auto handle_result_cb = [=] (const char * uri, const Index<char> & buf) {
        if (! buf.len ())
        {
            update_lyrics_window_error (str_printf (_("Unable to fetch %s"), uri));
            return;
        }

        String lyrics;
        if (! try_parse_json (buf, "plainLyrics", lyrics))
        {
            update_lyrics_window_error (str_printf (_("Unable to parse %s"), uri));
            return;
        }

        LyricsState new_state = g_state;
        new_state.lyrics = lyrics;

        if (! lyrics)
        {
            update_lyrics_window_notfound (new_state);
            return;
        }

        new_state.source = LyricsState::Source::LrcLib;

        update_lyrics_window (new_state.title, new_state.artist, new_state.lyrics);
        persist_state (new_state);
    };

    auto artist = str_copy (state.artist);
    artist = str_encode_percent (state.artist, -1);

    auto title = str_copy (state.title);
    title = str_encode_percent (state.title, -1);

    auto fetch_uri = str_concat(
        {m_base_url, "/api/get?artist_name=", artist, "&track_name=", title});

    vfs_async_file_get_contents (fetch_uri, handle_result_cb);
    update_lyrics_window_message (state, _("Looking for lyrics ..."));
}
