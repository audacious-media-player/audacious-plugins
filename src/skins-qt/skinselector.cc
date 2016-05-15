/*
 * ui_skinselector.c
 * Copyright 1998-2003 XMMS Development Team
 * Copyright 2003-2004 BMP Development Team
 * Copyright 2011 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include <stdlib.h>
#include <glib.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "plugin.h"
#include "skin.h"
#include "skinselector.h"
#include "util.h"

Index<SkinNode> skinlist;

#if 0
static GdkPixbuf * skin_get_preview (const char * path)
{
    GdkPixbuf * preview = nullptr;

    StringBuf archive_path;
    if (file_is_archive (path))
    {
        archive_path.steal (archive_decompress (path));
        if (! archive_path)
            return nullptr;

        path = archive_path;
    }

    StringBuf preview_path = skin_pixmap_locate (path, "main");
    if (preview_path)
        preview = gdk_pixbuf_new_from_file (preview_path, nullptr);

    if (archive_path)
        del_directory (archive_path);

    return preview;
}

static GdkPixbuf * skin_get_thumbnail (const char * path)
{
    StringBuf base = filename_get_base (path);
    base.insert (-1, ".png");

    StringBuf thumbname = filename_build ({skins_get_skin_thumb_dir (), base});
    GdkPixbuf * thumb = nullptr;

    if (g_file_test (thumbname, G_FILE_TEST_EXISTS))
        thumb = gdk_pixbuf_new_from_file (thumbname, nullptr);

    if (! thumb)
    {
        thumb = skin_get_preview (path);

        if (thumb)
        {
            make_directory (skins_get_skin_thumb_dir ());
            gdk_pixbuf_save (thumb, thumbname, "png", nullptr, nullptr);
        }
    }

    if (thumb)
        audgui_pixbuf_scale_within (& thumb, audgui_get_dpi () * 3 / 2);

    return thumb;
}
#endif

static void scan_skindir_func (const char * path, const char * basename)
{
    if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
    {
        if (file_is_archive (path))
            skinlist.append (String (archive_basename (basename)),
             String (_("Archived Winamp 2.x skin")), String (path));
    }
    else if (g_file_test (path, G_FILE_TEST_IS_DIR))
        skinlist.append (String (basename),
         String (_("Unarchived Winamp 2.x skin")), String (path));
}

void skinlist_update ()
{
    skinlist.clear ();

    const char * user_skin_dir = skins_get_user_skin_dir ();
    if (g_file_test (user_skin_dir, G_FILE_TEST_EXISTS))
        dir_foreach (user_skin_dir, scan_skindir_func);

    StringBuf path = filename_build ({aud_get_path (AudPath::DataDir), "Skins"});
    dir_foreach (path, scan_skindir_func);

    const char * skinsdir = getenv ("SKINSDIR");
    if (skinsdir)
    {
        for (const String & dir : str_list_to_index (skinsdir, ":"))
            dir_foreach (dir, scan_skindir_func);
    }

    skinlist.sort ([] (const SkinNode & a, const SkinNode & b)
        { return str_compare (a.name, b.name); });
}
