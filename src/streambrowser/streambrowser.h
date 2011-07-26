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


#ifndef STREAMBROWSER_H
#define STREAMBROWSER_H

#define DEF_STRING_LEN				1024
#define DEF_BUFFER_SIZE				512
#define MAX_UPDATE_THREADS			4
#define PLAYLIST_TEMP_FILE			"streambrowser-playlist-temp.pls"
#define STREAMBROWSER_ICON_SMALL	DATA_DIR G_DIR_SEPARATOR_S "images" G_DIR_SEPARATOR_S "streambrowser-16x16.png"
#define STREAMBROWSER_ICON			DATA_DIR G_DIR_SEPARATOR_S "images" G_DIR_SEPARATOR_S "streambrowser-64x64.png"


#include <glib.h>

#include <config.h>
#include <audacious/i18n.h>


void failure (const char *fmt, ...);
gboolean fetch_remote_to_local_file (gchar * remote_url, gchar * local_url);

void config_load ();
void config_save ();

        /* returns true if the substring has been found, false otherwise */
gboolean mystrcasestr (const char *haystack, const char *needle);

extern GMutex *update_streamdir_mutex; /* used to synchronize the access to streamdirs */

#endif // STREAMBROWSER_H

