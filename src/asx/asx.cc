/*
 * asx.c
 * Copyright 2010-2013 William Pitcock and John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <stdlib.h>
#include <string.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/inifile.h>

static const char * const asx_exts[] = {"asx"};

class ASXLoader : public PlaylistPlugin
{
public:
    static constexpr PluginInfo info = {N_("ASXv1/ASXv2 Playlists"), PACKAGE};

    constexpr ASXLoader () : PlaylistPlugin (info, asx_exts, false) {}

    bool load (const char * filename, VFSFile & file, String & title,
     Index<PlaylistAddItem> & items);
};

EXPORT ASXLoader aud_plugin_instance;

class ASXParser : public IniParser
{
public:
    ASXParser (const char * filename, Index<PlaylistAddItem> & items) :
        filename (filename),
        items (items),
        valid_heading (false) {}

private:
    const char * filename;
    Index<PlaylistAddItem> & items;
    bool valid_heading;

    void handle_heading (const char * heading)
        { valid_heading = ! strcmp_nocase (heading, "reference"); }

    void handle_entry (const char * key, const char * value)
    {
        if (! valid_heading || ! str_has_prefix_nocase (key, "ref"))
            return;

        StringBuf uri = uri_construct (value, filename);
        if (! uri)
            return;

        if (! strncmp ("http://", uri, 7))
            items.append (String (str_printf ("mms://%s", uri + 7)));
        else
            items.append (String (uri));
    }
};

bool ASXLoader::load (const char * filename, VFSFile & file, String & title,
 Index<PlaylistAddItem> & items)
{
    ASXParser (filename, items).parse (file);
    return (items.len () > 0);
}
