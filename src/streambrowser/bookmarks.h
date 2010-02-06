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


#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include "streambrowser.h"
#include "streamdir.h"

#define BOOKMARKS_NAME				"Bookmarks"
#define BOOKMARKS_ICON				DATA_DIR G_DIR_SEPARATOR_S "images" G_DIR_SEPARATOR_S "bookmarks.png"


typedef struct
{
    gchar streamdir_name[DEF_STRING_LEN];

    gchar name[DEF_STRING_LEN];
    gchar playlist_url[DEF_STRING_LEN];
    gchar url[DEF_STRING_LEN];
}
bookmark_t;


gboolean bookmarks_streaminfo_fetch (category_t * category,
                                     streaminfo_t * streaminfo);
gboolean bookmarks_category_fetch (streamdir_t * streamdir,
                                   category_t * category);
streamdir_t *bookmarks_streamdir_fetch (bookmark_t ** p_bookmarks,
                                        int *p_bookmarks_count);

void bookmark_add (bookmark_t * bookmark);
void bookmark_remove (gchar * name);


#endif // BOOKMARKS_H
