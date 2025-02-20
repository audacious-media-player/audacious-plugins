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

#ifndef AUDACIOUS_LYRICS_H
#define AUDACIOUS_LYRICS_H

#include <string.h>
#include <libxml/parser.h>

#define AUD_GLIB_INTEGRATION
#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/vfs.h>
#include <libaudcore/vfs_async.h>

struct LyricsState {
    String filename; // of song file
    String title, artist;
    String lyrics;

    enum Source {
        None,
        Embedded,
        Local,
        LyricsOVH,
        ChartLyrics,
        LrcLib
    } source = None;

    bool error = false;
};


// LyricProvider encapsulates an entire strategy for fetching lyrics,
// for example from chartlyrics.com, lyrics.ovh or local storage.
class LyricProvider
{
public:
    virtual bool match (LyricsState state) = 0;
    virtual void fetch (LyricsState state) = 0;
    virtual String edit_uri (LyricsState state) = 0;
};


// FileProvider provides a strategy for fetching and saving lyrics
// in local files.  It also manages the local lyrics cache.
class FileProvider : public LyricProvider
{
public:
    FileProvider () {};

    bool match (LyricsState state) override;
    void fetch (LyricsState state) override;
    String edit_uri (LyricsState state) override { return String (); }

    void save (LyricsState state);
    void cache (LyricsState state);
    void cache_fetch (LyricsState state);

private:
    String local_uri_for_entry (LyricsState state);
    String cache_uri_for_entry (LyricsState state);
};


// ChartLyricsProvider provides a strategy for fetching lyrics using the API
// from chartlyrics.com. It uses the two-step approach since the endpoint
// "SearchLyricDirect" may sometimes return incorrect data. One example is
// "Metallica - Unforgiven II" which leads to the lyrics of "Unforgiven".
class ChartLyricsProvider : public LyricProvider
{
public:
    ChartLyricsProvider () {};

    bool match (LyricsState state) override;
    void fetch (LyricsState state) override;
    String edit_uri (LyricsState state) override { return m_lyric_url; }

private:
    String match_uri (LyricsState state);
    String fetch_uri (LyricsState state);

    void reset_lyric_metadata ();
    bool has_match (LyricsState state, xmlNodePtr node);

    int m_lyric_id = -1;
    String m_lyric_checksum, m_lyric_url, m_lyrics;

    const char * m_base_url = "http://api.chartlyrics.com/apiv1.asmx";
};


// LrcLibProvider provides a strategy for fetching lyrics using the LRCLIB API.
class LrcLibProvider : public LyricProvider
{
public:
    LrcLibProvider () {};

    bool match (LyricsState state) override;
    void fetch (LyricsState state) override;
    String edit_uri (LyricsState state) override { return String (); }

private:
    const char * m_base_url = "https://lrclib.net";
};


// LyricsOVHProvider provides a strategy for fetching lyrics using the
// lyrics.ovh search engine.
class LyricsOVHProvider : public LyricProvider
{
public:
    LyricsOVHProvider () {};

    bool match (LyricsState state) override;
    void fetch (LyricsState state) override;
    String edit_uri (LyricsState state) override { return String (); }

private:
    const char * m_base_url = "https://api.lyrics.ovh";
};


extern FileProvider file_provider;
extern ChartLyricsProvider chart_lyrics_provider;
extern LrcLibProvider lrclib_provider;
extern LyricsOVHProvider lyrics_ovh_provider;
LyricProvider * remote_source ();

extern LyricsState g_state;
void persist_state (LyricsState state);

void update_lyrics_window (const char * title, const char * artist, const char * lyrics);
void update_lyrics_window_message (LyricsState state, const char * message);
void update_lyrics_window_error (const char * message);
void update_lyrics_window_notfound (LyricsState state);

bool try_parse_json (const Index<char> & buf, const char * key, String & output);

void lyrics_playback_began ();

#endif // AUDACIOUS_LYRICS_H
