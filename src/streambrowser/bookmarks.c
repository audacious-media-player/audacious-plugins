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

#include <audacious/debug.h>
#include <audacious/plugin.h>

#include "streambrowser.h"
#include "bookmarks.h"


static bookmark_t **bookmarks;
static int *bookmarks_count;

gboolean bookmarks_streaminfo_fetch (category_t * category,
                                     streaminfo_t * streaminfo)
{
    // todo: implement and make use of this

    return FALSE;
}

gboolean bookmarks_category_fetch (streamdir_t * streamdir,
                                   category_t * category)
{
    AUDDBG("bookmarks: filling category '%s'\n", category->name);

    /* free/remove any existing streaminfos in this category */
    while (streaminfo_get_count (category) > 0)
        streaminfo_remove (category, streaminfo_get_by_index (category, 0));

    int i;
    /* find bookmarks that match this category */
    for (i = 0; i < *bookmarks_count; i++)
        if (strcmp ((*bookmarks)[i].streamdir_name, category->name) == 0)
        {
            AUDDBG("bookmarks: adding stream info for '%s/%s'\n",
                   streamdir->name, category->name);

            streaminfo_t *streaminfo = streaminfo_new ((*bookmarks)[i].name,
                                                       (*bookmarks)[i].
                                                       playlist_url,
                                                       (*bookmarks)[i].url, "");
            streaminfo_add (category, streaminfo);

            AUDDBG("bookmarks: stream info added\n");
        }

    return TRUE;
}

streamdir_t *bookmarks_streamdir_fetch (bookmark_t ** p_bookmarks,
                                        int *p_bookmarks_count)
{
    bookmarks = p_bookmarks;
    bookmarks_count = p_bookmarks_count;

    streamdir_t *streamdir = streamdir_new (BOOKMARKS_NAME);

    AUDDBG("bookmarks: creating streaming directory for bookmarks\n");

    category_t *category = category_new ("Shoutcast");
    category_add (streamdir, category);

    category = category_new ("Xiph");
    category_add (streamdir, category);

    AUDDBG("bookmarks: streaming directory successfuly created\n");

    return streamdir;
}

void bookmark_add (bookmark_t * bookmark)
{
    AUDDBG
        ("bookmarks: adding bookmark with streamdir = '%s', name = '%s', playlist_url = '%s', url = '%s'\n",
         bookmark->streamdir_name, bookmark->name, bookmark->playlist_url,
         bookmark->url);

    int i;
    for (i = 0; i < *bookmarks_count; i++)
        if (strcmp ((*bookmarks)[i].name, bookmark->name) == 0)
        {
            AUDDBG
                ("bookmarks: bookmark with name = '%s' already exists, skipping\n",
                 bookmark->name);
            return;
        }

    *bookmarks =
        realloc (*bookmarks, sizeof (bookmark_t) * ((*bookmarks_count) + 1));

    strncpy ((*bookmarks)[*bookmarks_count].streamdir_name,
             bookmark->streamdir_name, DEF_STRING_LEN);
    strncpy ((*bookmarks)[*bookmarks_count].name, bookmark->name,
             DEF_STRING_LEN);
    strncpy ((*bookmarks)[*bookmarks_count].playlist_url,
             bookmark->playlist_url, DEF_STRING_LEN);
    strncpy ((*bookmarks)[*bookmarks_count].url, bookmark->url, DEF_STRING_LEN);

    (*bookmarks_count)++;

    AUDDBG("bookmarks: bookmark added, there are now %d bookmarks\n",
           *bookmarks_count);

    /* issue a configuration save for immediately saving the new added bookmark */
    config_save ();
}

void bookmark_remove (gchar * name)
{
    AUDDBG("bookmarks: searching for bookmark with name = '%s'\n", name);

    int pos = -1, i;

    for (i = 0; i < *bookmarks_count; i++)
        if (strcmp ((*bookmarks)[i].name, name) == 0)
        {
            pos = i;
            break;
        }

    if (pos != -1)
    {
        AUDDBG
            ("bookmarks: removing bookmark with streamdir = '%s', name = '%s', playlist_url = '%s', url = '%s'\n",
             (*bookmarks)[i].streamdir_name, (*bookmarks)[i].name,
             (*bookmarks)[i].playlist_url, (*bookmarks)[i].url);

        for (i = pos; i < (*bookmarks_count) - 1; i++)
        {
            /* bookmarks[i] = bookmarks[i + 1] */

            strncpy ((*bookmarks)[i].streamdir_name,
                     (*bookmarks)[i + 1].streamdir_name, DEF_STRING_LEN);
            strncpy ((*bookmarks)[i].name, (*bookmarks)[i + 1].name,
                     DEF_STRING_LEN);
            strncpy ((*bookmarks)[i].playlist_url,
                     (*bookmarks)[i + 1].playlist_url, DEF_STRING_LEN);
            strncpy ((*bookmarks)[i].url, (*bookmarks)[i + 1].url,
                     DEF_STRING_LEN);
        }

        (*bookmarks_count)--;
        if (*bookmarks_count > 0)
            *bookmarks =
                realloc (*bookmarks, sizeof (bookmark_t) * (*bookmarks_count));
        else
            *bookmarks = NULL;

        AUDDBG("bookmarks: bookmark removed, there are now %d bookmarks\n",
               *bookmarks_count);
    }
    else
        failure ("bookmarks: cannot find a bookmark with name = '%s'\n", name);

    /* issue a configuration save for immediately saving the remaining bookmarks */
    config_save ();
}
