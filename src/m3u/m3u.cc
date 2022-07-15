/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2006-2010 William Pitcock, Tony Vroon, George Averill, Giacomo
 *  Lozito, Derek Pomery and Yoshiki Yazawa, and John Lindgren.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <string.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

static const char * const m3u_exts[] = {"m3u", "m3u8"};

class M3ULoader : public PlaylistPlugin
{
public:
    static constexpr PluginInfo info = {N_("M3U Playlists"), PACKAGE};

    constexpr M3ULoader () : PlaylistPlugin (info, m3u_exts, true) {}

    bool load (const char * filename, VFSFile & file, String & title,
     Index<PlaylistAddItem> & items);
    bool save (const char * filename, VFSFile & file, const char * title,
     const Index<PlaylistAddItem> & items);
};

EXPORT M3ULoader aud_plugin_instance;

static char * split_line (char * line)
{
    char * feed = strchr (line, '\n');
    if (! feed)
        return nullptr;

    if (feed > line && feed[-1] == '\r')
        feed[-1] = 0;
    else
        feed[0] = 0;

    return feed + 1;
}

bool M3ULoader::load (const char * filename, VFSFile & file, String & title,
 Index<PlaylistAddItem> & items)
{
    Index<char> text = file.read_all ();
    if (! text.len ())
        return false;

    text.append (0);  /* null-terminate */

    bool firstline = true;
    bool extm3u = false;

    char * parse = text.begin ();
    if (! strncmp (parse, "\xef\xbb\xbf", 3)) /* byte order mark */
        parse += 3;

    while (parse)
    {
        char * next = split_line (parse);

        while (* parse == ' ' || * parse == '\t')
            parse ++;

        if (* parse == '#')
        {
            if (firstline && ! strncmp (parse, "#EXTM3U", 7))
                extm3u = true;
            else if (extm3u && ! strncmp (parse, "#EXT-X-", 7))
                goto HLS;
        }
        else if (* parse)
        {
            StringBuf s = uri_construct (parse, filename);
            if (s)
                items.append (String (s));
        }

        firstline = false;
        parse = next;
    }

    return true;

HLS:
    AUDINFO ("Detected HLS stream: %s\n", filename);
    /* Though it shares the .m3u8 extension, an HLS stream is not really
     * a playlist at all.  Add (only) the original .m3u8 filename to the
     * playlist and let an input plugin (e.g. ffaudio) handle it. */
    items.clear ();
    items.append (String (filename));
    return true;
}

bool M3ULoader::save (const char * filename, VFSFile & file, const char * title,
 const Index<PlaylistAddItem> & items)
{
    for (auto & item : items)
    {
        StringBuf path = uri_deconstruct (item.filename, filename);
        StringBuf line = str_concat ({path, "\n"});
        if (file.fwrite (line, 1, line.len ()) != line.len ())
            return false;
    }

    return true;
}
