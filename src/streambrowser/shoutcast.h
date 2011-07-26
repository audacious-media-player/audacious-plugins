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


#ifndef SHOUTCAST_H
#define SHOUTCAST_H

#include "streambrowser.h"
#include "streamdir.h"

#define SHOUTCAST_NAME				"Shoutcast"
#define SHOUTCAST_ICON				DATA_DIR G_DIR_SEPARATOR_S "images" G_DIR_SEPARATOR_S "shoutcast.png"
#define SHOUTCAST_STREAMDIR_URL		"http://yp.shoutcast.com/sbin/newxml.phtml"
#define SHOUTCAST_CATEGORY_URL		"http://yp.shoutcast.com/sbin/newxml.phtml?genre=%s"
#define SHOUTCAST_STREAMINFO_URL	"http://yp.shoutcast.com/sbin/shoutcast-playlist.pls?rn=%s&file=filename.pls"


gboolean shoutcast_streaminfo_fetch (category_t * category,
                                     streaminfo_t * streaminfo);
gboolean shoutcast_category_fetch (streamdir_t * streamdir,
                                   category_t * category);
streamdir_t *shoutcast_streamdir_fetch ();


#endif // SHOUTCAST_H
