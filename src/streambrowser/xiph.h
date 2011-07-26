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


#ifndef XIPH_H
#define XIPH_H

#include "streambrowser.h"
#include "streamdir.h"

#define XIPH_NAME				"Xiph"
#define XIPH_ICON				DATA_DIR G_DIR_SEPARATOR_S "images" G_DIR_SEPARATOR_S "xiph.png"
#define XIPH_STREAMDIR_URL		"http://dir.xiph.org/yp.xml"
#define XIPH_TEMP_FILENAME		"streambrowser-xiph-temp.xml"


gboolean xiph_streaminfo_fetch (category_t * category,
                                streaminfo_t * streaminfo);
gboolean xiph_category_fetch (streamdir_t * streamdir, category_t * category);
streamdir_t *xiph_streamdir_fetch ();


#endif // SHOUTCAST_H
