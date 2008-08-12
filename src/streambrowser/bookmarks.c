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


#include <string.h>
#include <glib.h>
#include <audacious/plugin.h>

#include "streambrowser.h"
#include "bookmarks.h"


static bookmark_t *bookmarks;
static int bookmarks_count;

gboolean bookmarks_streaminfo_fetch(category_t *category, streaminfo_t *streaminfo)
{
}

gboolean bookmarks_category_fetch(streamdir_t *streamdir, category_t *category)
{
	debug("bookmarks: filling category '%s'\n", category->name);
	
	/* free/remove any existing streaminfos in this category */
	while (streaminfo_get_count(category) > 0)
		streaminfo_remove(category, streaminfo_get_by_index(category, 0));

	int i;
	/* find bookmarks that match this category */
	for (i = 0; i < bookmarks_count; i++)
		if (strcmp(bookmarks[i].streamdir_name, streamdir->name) == 0 &&
			strcmp(bookmarks[i].category_name, category->name) == 0) {

			debug("bookmarks: adding stream info for '%s/%d'\n", streamdir->name, category->name);

			streaminfo_t *streaminfo = streaminfo_new(bookmarks[i].name, bookmarks[i].playlist_url, bookmarks[i].url, "");
			streaminfo_add(category, streaminfo);
			
			debug("bookmarks: stream info added\n");
		}

	return TRUE;
}

streamdir_t* bookmarks_streamdir_fetch(bookmark_t *bms, int count)
{
	bookmarks = bms;
	bookmarks_count = count;
	
	streamdir_t *streamdir = streamdir_new(BOOKMARKS_NAME);
	
	debug("bookmarks: creating streaming directory for bookmarks\n");

	category_t *category = category_new("Shoutcast");
	category_add(streamdir, category);

	category = category_new("Xiph");
	category_add(streamdir, category);

	debug("bookmarks: streaming directory successfuly created\n");

	return streamdir;
}

