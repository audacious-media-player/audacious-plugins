/*  Audacious GNT interface
 *  Copyright (C) 2009 Tomasz Moń <desowin@gmail.com>
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
 */

#ifndef GNTUI_FILESELECTOR_H
#define GNTUI_FILESELECTOR_H

#include <glib.h>

void run_fileselector(gboolean play_button);
void open_files();
void add_files();
void hide_fileselector();

#endif /* GNTUI_FILESELECTOR_H */
