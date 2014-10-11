/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2006 William Pitcock, Tony Vroon, George Averill,
 *                    Giacomo Lozito, Derek Pomery and Yoshiki Yazawa.
 * Copyright (c) 2011-2013 John Lindgren
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

#include <stdlib.h>
#include <string.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/inifile.h>

static const char * const pls_exts[] = {"pls"};

class PLSLoader : public PlaylistPlugin
{
public:
    static constexpr PluginInfo info = {N_("PLS Playlists"), PACKAGE};

    constexpr PLSLoader () : PlaylistPlugin (info, pls_exts, true) {}

    bool load (const char * filename, VFSFile & file, String & title,
     Index<PlaylistAddItem> & items);
    bool save (const char * filename, VFSFile & file, const char * title,
     const Index<PlaylistAddItem> & items);

private:
    struct LoadState {
        const char * filename;
        bool valid_heading;
        Index<PlaylistAddItem> & items;
    };

    static void handle_heading (const char * heading, void * data);
    static void handle_entry (const char * key, const char * value, void * data);
};

EXPORT PLSLoader aud_plugin_instance;

void PLSLoader::handle_heading (const char * heading, void * data)
{
    LoadState * state = (LoadState *) data;

    state->valid_heading = ! strcmp_nocase (heading, "playlist");
}

void PLSLoader::handle_entry (const char * key, const char * value, void * data)
{
    LoadState * state = (LoadState *) data;

    if (! state->valid_heading || strcmp_nocase (key, "file", 4))
        return;

    StringBuf uri = uri_construct (value, state->filename);
    if (uri)
        state->items.append (String (uri));
}

bool PLSLoader::load (const char * filename, VFSFile & file, String & title,
 Index<PlaylistAddItem> & items)
{
    LoadState state = {
        filename,
        false,
        items
    };

    inifile_parse (file, handle_heading, handle_entry, & state);

    return (items.len () > 0);
}

bool PLSLoader::save (const char * filename, VFSFile & file, const char * title,
 const Index<PlaylistAddItem> & items)
{
    int entries = items.len ();

    StringBuf header = str_printf ("[playlist]\nNumberOfEntries=%d\n", entries);
    if (file.fwrite (header, 1, header.len ()) != header.len ())
        return false;

    for (int count = 0; count < entries; count ++)
    {
        const char * uri = items[count].filename;
        StringBuf local = uri_to_filename (uri);
        StringBuf line = str_printf ("File%d=%s\n", 1 + count, local ? local : uri);
        if (file.fwrite (line, 1, line.len ()) != line.len ())
            return false;
    }

    return true;
}
