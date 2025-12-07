/*
 * Copyright (c) 2010-2019 Ariadne Conill <ariadne@dereferenced.org>
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

#include <errno.h>
#include <string.h>

#include <glib/gstdio.h>

#include "lyrics.h"
#include "preferences.h"

void FileProvider::cache (LyricsState state)
{
    auto uri = cache_uri_for_entry (state);
    if (! uri)
        return;

    bool exists = VFSFile::test_file (uri, VFS_IS_REGULAR);
    if (exists)
        return;

    AUDINFO ("Add to cache: %s\n", (const char *) uri);
    VFSFile::write_file (uri, state.lyrics, strlen (state.lyrics));
}

#ifdef S_IRGRP
#define DIRMODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
#else
#define DIRMODE (S_IRWXU)
#endif

String FileProvider::cache_uri_for_entry (LyricsState state)
{
    if (! state.artist)
        return String ();

    auto user_dir = aud_get_path (AudPath::UserDir);
    StringBuf base_path = filename_build ({user_dir, "lyrics"});
    StringBuf artist_path = filename_build ({base_path, state.artist});

    if (aud_get_bool (CFG_SECTION, "enable-cache"))
    {
        if (g_mkdir_with_parents (artist_path, DIRMODE) < 0)
            AUDERR ("Failed to create '%s': %s\n", (const char *) artist_path, strerror (errno));
    }

    StringBuf title_path = str_concat ({filename_build({artist_path, state.title}), ".lrc"});

    return String (filename_to_uri (title_path));
}

String FileProvider::local_uri_for_entry (LyricsState state)
{
    if (strcmp (uri_get_scheme (state.filename), "file"))
        return String ();

    // it's a local file: convert our URI to a local path
    StringBuf filename = uri_to_filename (state.filename);

    // strip off the extension
    char * ext = strrchr ((char *) filename, '.');
    if (! ext)
        return String ();
    * ext = '\0';

    // combine the mangled filename and '.lrc' extension
    return String (filename_to_uri (str_concat ({filename, ".lrc"})));
}

void FileProvider::fetch (LyricsState state)
{
    String path = local_uri_for_entry (state);
    if (! path)
        return;

    auto data = VFSFile::read_file (path, VFS_APPEND_NULL);
    if (! data.len ())
        return;

    state.lyrics = String (data.begin ());
    state.source = LyricsState::Source::Local;

    update_lyrics_window (state.title, state.artist, state.lyrics);
    persist_state (state);
}

void FileProvider::cache_fetch (LyricsState state)
{
    String path = cache_uri_for_entry (state);
    if (! path)
        return;

    auto data = VFSFile::read_file (path, VFS_APPEND_NULL);
    if (! data.len ())
        return;

    state.lyrics = String (data.begin ());
    state.source = LyricsState::Source::Local;

    update_lyrics_window (state.title, state.artist, state.lyrics);
    persist_state (state);
}

bool FileProvider::match (LyricsState state)
{
    String path = local_uri_for_entry (state);
    if (! path)
        return false;

    AUDINFO ("Checking for local lyric file: '%s'\n", (const char *) path);

    bool exists = VFSFile::test_file (path, VFS_IS_REGULAR);
    if (exists)
    {
        fetch (state);
        return true;
    }

    path = cache_uri_for_entry (state);
    if (! path)
        return false;

    AUDINFO ("Checking for cache lyric file: '%s'\n", (const char *) path);

    exists = VFSFile::test_file (path, VFS_IS_REGULAR);
    if (exists)
        cache_fetch (state);

    return exists;
}

void FileProvider::save (LyricsState state)
{
    if (! state.lyrics)
        return;

    String path = local_uri_for_entry (state);
    if (! path)
        return;

    AUDINFO ("Saving lyrics to local file: '%s'\n", (const char *) path);

    VFSFile::write_file (path, state.lyrics, strlen (state.lyrics));
}
