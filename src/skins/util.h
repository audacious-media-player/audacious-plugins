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

#include <libaudcore/vfs.h>

typedef void (* DirForeachFunc) (const char * path, const char * basename);

StringBuf find_file_case_path (const char * folder, const char * basename);

VFSFile open_local_file_nocase (const char * folder, const char * basename);
StringBuf skin_pixmap_locate (const char * folder, const char * basename,
 const char * altname = nullptr);

char * text_parse_line (char * text);

void make_directory (const char * path);
void del_directory (const char * path);

bool dir_foreach (const char * path, DirForeachFunc func);

Index<int> string_to_int_array (const char *str);

bool file_is_archive (const char * filename);
StringBuf archive_basename (const char * str);
StringBuf archive_decompress (const char * path);

#endif
