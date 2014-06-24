/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2009  Audacious development team
 *
 *  Based on BMP:
 *  Copyright (C) 2003-2004  BMP development team
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#ifndef UTIL_H
#define UTIL_H

#include <glib.h>

#include <libaudcore/vfs.h>

typedef gboolean(*DirForeachFunc) (const char *path, const char *basename,
                                   gpointer user_data);

char * find_file_case (const char * folder, const char * basename);
char * find_file_case_path (const char * folder, const char * basename);

VFSFile * open_local_file_nocase (const char * folder, const char * basename);

char * text_parse_line (char * text);

void del_directory(const char *dirname);
gboolean dir_foreach(const char *path, DirForeachFunc function,
                     gpointer user_data, GError **error);

GArray *string_to_garray(const char *str);

gboolean file_is_archive(const char *filename);
char *archive_decompress(const char *path);
char *archive_basename(const char *path);

#endif
