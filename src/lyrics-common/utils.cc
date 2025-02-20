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

#include "lyrics.h"
#include "preferences.h"

LyricProvider * remote_source ()
{
    String source = aud_get_str (CFG_SECTION, "remote-source");

    if (! strcmp (source, "chartlyrics.com"))
        return & chart_lyrics_provider;

    if (! strcmp (source, "lrclib.net"))
        return & lrclib_provider;

    if (! strcmp (source, "lyrics.ovh"))
        return & lyrics_ovh_provider;

    return nullptr;
}

void persist_state (LyricsState state)
{
    g_state = state;
    g_state.error = false;

    if (g_state.source != LyricsState::Source::Local && aud_get_bool (CFG_SECTION, "enable-cache"))
        file_provider.cache (state);
}

void update_lyrics_window_message (LyricsState state, const char * message)
{
    update_lyrics_window (state.title, state.artist, message);
}

void update_lyrics_window_error (const char * message)
{
    update_lyrics_window (_("Error"), nullptr, message);
    g_state.error = true;
}

void update_lyrics_window_notfound (LyricsState state)
{
    update_lyrics_window (state.title, state.artist, _("Lyrics could not be found."));
    g_state.error = true;
}

static char * truncate_by_pattern (const char * input, const char * pattern)
{
    GRegex * regex = g_regex_new (pattern, G_REGEX_CASELESS, (GRegexMatchFlags) 0, nullptr);
    char * result = g_regex_replace (regex, input, -1, 0, "", (GRegexMatchFlags) 0, nullptr);
    g_regex_unref (regex);
    return result;
}

static void split_title_and_truncate ()
{
    StringBuf split_pattern = str_concat ({
        "^(.*)\\s+[", aud_get_str (CFG_SECTION, "split-on-chars"), "]\\s+(.*)$"
    });

    GMatchInfo * match_info;
    GRegex * split_regex = g_regex_new (split_pattern, G_REGEX_CASELESS, (GRegexMatchFlags) 0, nullptr);

    if (g_regex_match (split_regex, g_state.title, (GRegexMatchFlags) 0, & match_info))
    {
        CharPtr artist (g_match_info_fetch (match_info, 1));
        CharPtr title (g_match_info_fetch (match_info, 2));

        if (aud_get_bool (CFG_SECTION, "truncate-fields-on-chars"))
        {
            StringBuf artist_pattern = str_concat ({
                "^.*\\s+[", aud_get_str (CFG_SECTION, "truncate-on-chars"), "]\\s+"
            });

            StringBuf title_pattern = str_concat ({
                "\\s+[", aud_get_str (CFG_SECTION, "truncate-on-chars"), "]\\s+.*$"
            });

            artist = CharPtr (truncate_by_pattern (artist, artist_pattern));
            title = CharPtr (truncate_by_pattern (title, title_pattern));
        }

        g_state.artist = String ();
        g_state.title = String ();
        g_state.artist = String (artist);
        g_state.title = String (title);
    }

    g_match_info_free (match_info);
    g_regex_unref (split_regex);
}

void lyrics_playback_began ()
{
    // FIXME: Cancel previous VFS requests (not possible with current API)

    g_state.filename = aud_drct_get_filename ();

    Tuple tuple = aud_drct_get_tuple ();
    g_state.title = tuple.get_str (Tuple::Title);
    g_state.artist = tuple.get_str (Tuple::Artist);
    g_state.lyrics = String ();

    if (aud_get_bool (CFG_SECTION, "use-embedded"))
    {
        String embedded_lyrics = tuple.get_str (Tuple::Lyrics);
        if (embedded_lyrics && embedded_lyrics[0])
        {
            g_state.lyrics = embedded_lyrics;
            g_state.source = LyricsState::Source::Embedded;
            g_state.error = false;

            update_lyrics_window (g_state.title, g_state.artist, g_state.lyrics);
            return;
        }
    }

    if (aud_get_bool (CFG_SECTION, "split-title-on-chars"))
        split_title_and_truncate ();

    if (! aud_get_bool (CFG_SECTION, "enable-file-provider") || ! file_provider.match (g_state))
    {
        if (! g_state.artist || ! g_state.title)
        {
            update_lyrics_window_error (_("Missing title and/or artist."));
            return;
        }

        LyricProvider * remote_provider = remote_source ();
        if (remote_provider)
        {
            remote_provider->match (g_state);
            return;
        }
    }

    if (! g_state.lyrics)
        update_lyrics_window_notfound (g_state);
}
