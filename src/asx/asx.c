/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2006 William Pitcock, Tony Vroon, George Averill,
 *                    Giacomo Lozito, Derek Pomery and Yoshiki Yazawa.
 * Copyright (c) 2011 John Lindgren
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

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

#include "util.h"
#include "asxv3.h"

/* TODO: decide which version to write */
#define WRITE_OLD_VERSION TRUE

static gboolean playlist_load_asx (const gchar * filename, VFSFile * file,
 gchar * * title, Index * filenames, Index * tuples)
{
	if (playlist_load_asx3(filename, file, title, filenames, tuples))
	{
		
		return TRUE;
	}

	/* Loading ver. 3 failed, so try ver. 1/2 */

    gint i;
    gchar line_key[16];
    gchar * line;

    INIFile * inifile = open_ini_file (file);

    * title = NULL;

    for (i = 1; ; i++) {
        g_snprintf(line_key, sizeof(line_key), "Ref%d", i);
        if ((line = read_ini_string(inifile, "Reference", line_key)))
        {
            gchar *uri;

            uri = aud_construct_uri(line, filename);
            g_free(line);

            if (!g_ascii_strncasecmp("http://", uri, 7))
                uri = str_replace_fragment(uri, strlen(uri), "http://", "mms://");

            if (uri != NULL)
                index_append (filenames, str_get (uri));

            g_free (uri);
        }
        else
            break;
    }

    close_ini_file(inifile);
    return TRUE;
}

static gboolean playlist_save_asx (const gchar * filename, VFSFile * file,
 const gchar * title, Index * filenames, Index * tuples)
{
	if (WRITE_OLD_VERSION)
	{
		gint entries = index_count (filenames);
		gint count;

		vfs_fprintf(file, "[Reference]\r\n");

		for (count = 0; count < entries; count ++)
		{
		    const gchar * filename = index_get (filenames, count);
		    gchar *fn;

		    if (vfs_is_remote (filename))
		        fn = g_strdup (filename);
		    else
		        fn = g_filename_from_uri (filename, NULL, NULL);

		    vfs_fprintf (file, "Ref%d=%s\r\n", 1 + count, fn);
		    g_free(fn);
		}

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static const gchar * const asx_exts[] = {"asx", NULL};

static const char asx_about[] =
 N_("ASX Playlists v. 1.1\n\n"
	"This container plugin supports the reading\n"
    "and writing of asx v1-v3 files.");

AUD_PLAYLIST_PLUGIN
(
 .name = N_("ASX Playlists"),
 .domain = PACKAGE,
 .about_text = asx_about,
 .extensions = asx_exts,
 .load = playlist_load_asx,
 .save = playlist_save_asx
)

