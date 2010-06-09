/*
 * Audacious Streambrowser Plugin
 *
 * Copyright (c) 2008 Calin Crisan <ccrisan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 */


#ifndef STREAMDIR_H
#define STREAMDIR_H

#include <glib.h>

#include "streambrowser.h"


typedef struct
{
    gchar name[DEF_STRING_LEN];
    gchar playlist_url[DEF_STRING_LEN];
    gchar url[DEF_STRING_LEN];
    gchar current_track[DEF_STRING_LEN];
}
streaminfo_t;

typedef struct
{
    gchar name[DEF_STRING_LEN];
    GList *streaminfo_list;
}
category_t;

typedef struct
{
    gchar name[DEF_STRING_LEN];
    GList *category_list;
}
streamdir_t;

gboolean streamdir_is_valid(streamdir_t *streamdir);

streamdir_t *streamdir_new (gchar * name);
void streamdir_delete (streamdir_t * streamdir);

category_t *category_new (gchar * name);
void category_delete (category_t * category);
void category_add (streamdir_t * streamdir, category_t * category);
void category_remove (streamdir_t * streamdir, category_t * category);
category_t *category_get_by_index (streamdir_t * streamdir, gint index);
category_t *category_get_by_name (streamdir_t * streamdir, gchar * name);
gint category_get_count (streamdir_t * streamdir);
gint category_get_index (streamdir_t * streamdir, category_t * category);

streaminfo_t *streaminfo_new (gchar * name, gchar * playlist_url, gchar * url,
                              gchar * current_track);
void streaminfo_delete (streaminfo_t * streaminfo);
void streaminfo_free (streaminfo_t * streaminfo);
void streaminfo_add (category_t * category, streaminfo_t * streaminfo);
void streaminfo_remove (category_t * category, streaminfo_t * streaminfo);
streaminfo_t *streaminfo_get_by_index (category_t * category, gint index);
streaminfo_t *streaminfo_get_by_name (category_t * category, gchar * name);
gint streaminfo_get_count (category_t * category);
gint streaminfo_get_index (category_t * category, streaminfo_t * streaminfo);


#endif // STREAMDIR_H
