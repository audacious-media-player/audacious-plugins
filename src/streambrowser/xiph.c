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
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <audacious/debug.h>
#include <audacious/misc.h>

#include "streambrowser.h"
#include "xiph.h"


typedef struct
{
    gchar name[DEF_STRING_LEN];
    gchar url[DEF_STRING_LEN];
    gchar current_song[DEF_STRING_LEN];
    gchar genre[DEF_STRING_LEN];
}
xiph_entry_t;


static xiph_entry_t *xiph_entries = NULL;
static int xiph_entry_count = 0;

typedef struct
{
    gchar *name;
    gchar *match_string;
}
xiph_category_t;

/* inspired from streamtuner's xiph plugin */
static xiph_category_t xiph_categories[] =
{
    {"Alternative",
     "alternative indie goth college industrial punk hardcore ska"},
    {"Electronic",
     "electronic ambient drum trance techno house downtempo breakbeat jungle garage"},
    {"Classical", "classical opera symphonic"},
    {"Country", "country swing"},
    {"Hip-Hop/Rap", "hip hop rap turntable"},
    {"Jazz", "jazz swing"},
    {"Oldies", "oldies disco 50s 60s 70s 80s 90s"},
    {"Rop", "top pop"},
    {"Rock", "rock metal"},
    {"R&B/Soul", "r&b funk soul urban"},
    {"Spiritual", "spiritual gospel christian muslim jewish religio"},
    {"Spoken", "spoken talk comedy"},
    {"World", "world reggae island african european east asia"},
    {"Other", "various mixed misc eclectic film show instrumental"}
};


static void refresh_streamdir (void);
        /* returns true if any of the words in string1 is present in string2 */
static gboolean genre_match (gchar * string1, gchar * string2);

gboolean xiph_streaminfo_fetch (category_t * category,
                                streaminfo_t * streaminfo)
{
    gint entryno;

    refresh_streamdir ();

    /* find the corresponding xiph entry */
    for (entryno = 0; entryno < xiph_entry_count; entryno++)
    {
        if (strcmp (xiph_entries[entryno].name, streaminfo->name) == 0)
        {
            strcpy (streaminfo->name, xiph_entries[entryno].name);
            strcpy (streaminfo->url, xiph_entries[entryno].url);
            strcpy (streaminfo->current_track,
                    xiph_entries[entryno].current_song);

            break;
        }
    }

    return TRUE;
}

gboolean xiph_category_fetch (streamdir_t * streamdir, category_t * category)
{
    refresh_streamdir ();
    
    gint entryno, categoryno;
    gint xiph_category_count =
        sizeof (xiph_categories) / sizeof (xiph_category_t);
    xiph_category_t *xiph_category = NULL;

    for (categoryno = 0; categoryno < xiph_category_count; categoryno++) {
        if (strcmp (xiph_categories[categoryno].name, category->name) == 0)
        {
            xiph_category = xiph_categories + categoryno;
            break;
        }
    }

    /* somehow we've got an invalid/unrecognized category */
    if (xiph_category == NULL)
    {
        failure ("xiph: got an unrecognized category: '%s'\n", category->name);
        return FALSE;
    }

    /* free/remove any existing streaminfos in this category */
    while (streaminfo_get_count (category) > 0)
        streaminfo_remove (category, streaminfo_get_by_index (category, 0));

    /* see what entries match this category */
    for (entryno = 0; entryno < xiph_entry_count; entryno++)
    {
        if (genre_match
            (xiph_category->match_string, xiph_entries[entryno].genre))
        {
            streaminfo_t *streaminfo =
                streaminfo_new (xiph_entries[entryno].name, "",
                                xiph_entries[entryno].url,
                                xiph_entries[entryno].current_song);
            streaminfo_add (category, streaminfo);
        }
    }

    /* if the requested category is the last one in the list ("other"),
       we fill it with all the entries that don't match the rest of categories */
    if (xiph_category == &xiph_categories[xiph_category_count - 1])
    {
        for (entryno = 0; entryno < xiph_entry_count; entryno++)
        {
            gboolean matched = FALSE;

            for (categoryno = 0; categoryno < xiph_category_count; categoryno++)
                if (genre_match
                    (xiph_entries[entryno].genre,
                     xiph_categories[categoryno].match_string))
                {
                    matched = TRUE;
                    break;
                }

            if (!matched)
            {
                streaminfo_t *streaminfo =
                    streaminfo_new (xiph_entries[entryno].name, "",
                                    xiph_entries[entryno].url,
                                    xiph_entries[entryno].current_song);
                streaminfo_add (category, streaminfo);
            }
        }
    }

    return TRUE;
}


streamdir_t *xiph_streamdir_fetch (void)
{
    streamdir_t *streamdir = streamdir_new (XIPH_NAME);
    gint categno;

    refresh_streamdir ();
    
    for (categno = 0;
         categno < sizeof (xiph_categories) / sizeof (xiph_category_t);
         categno++)
    {
        category_t *category = category_new (xiph_categories[categno].name);
        category_add (streamdir, category);
    }

    return streamdir;
}

static void refresh_streamdir (void)
{
    gchar * unix_name = g_build_filename (aud_util_get_localdir (),
     XIPH_TEMP_FILENAME, NULL);
    gchar * uri_name = g_filename_to_uri (unix_name, NULL, NULL);

    /* free any previously fetched streamdir data */
    if (xiph_entries != NULL)
    {
        free (xiph_entries);
        xiph_entries = NULL;
    }
    xiph_entry_count = 0;

    AUDDBG("xiph: fetching streaming directory file '%s'\n",
           XIPH_STREAMDIR_URL);

    if (! fetch_remote_to_local_file (XIPH_STREAMDIR_URL, uri_name))
    {
        failure ("xiph: stream directory file '%s' could not be downloaded to "
         "'%s'\n", XIPH_STREAMDIR_URL, uri_name);
        goto DONE;
    }

    AUDDBG("xiph: stream directory file '%s' successfuly downloaded to '%s'\n",
     XIPH_STREAMDIR_URL, uri_name);

    xmlDoc * doc = xmlReadFile (uri_name, NULL, 0);

    if (doc == NULL)
    {
        failure ("xiph: failed to read stream directory file\n");
        goto DONE;
    }

    xmlNode *root_node = xmlDocGetRootElement (doc);
    xmlNode *node, *child;
    gchar *content;

    root_node = root_node->children;

    for (node = root_node; node != NULL; node = node->next)
    {
        if (node->type == XML_ELEMENT_NODE)
        {
            xiph_entries =
                realloc (xiph_entries,
                         sizeof (xiph_entry_t) * (xiph_entry_count + 1));

            for (child = node->children; child != NULL; child = child->next)
            {
                if (strcmp ((gchar *) child->name, "server_name") == 0)
                {
                    content = (gchar *) xmlNodeGetContent (child);
                    strcpy (xiph_entries[xiph_entry_count].name, content);
                    xmlFree (content);
                }
                else if (strcmp ((gchar *) child->name, "listen_url") == 0)
                {
                    content = (gchar *) xmlNodeGetContent (child);
                    strcpy (xiph_entries[xiph_entry_count].url, content);
                    xmlFree (content);
                }
                else if (strcmp ((gchar *) child->name, "current_song") == 0)
                {
                    content = (gchar *) xmlNodeGetContent (child);
                    strcpy (xiph_entries[xiph_entry_count].current_song,
                            content);
                    xmlFree (content);
                }
                else if (strcmp ((gchar *) child->name, "genre") == 0)
                {
                    content = (gchar *) xmlNodeGetContent (child);
                    strcpy (xiph_entries[xiph_entry_count].genre, content);
                    xmlFree (content);
                }
            }

            xiph_entry_count++;
        }
    }

    xmlFreeDoc (doc);

    AUDDBG("xiph: streaming directory successfuly loaded\n");

DONE:
    g_free (unix_name);
    g_free (uri_name);
}

static gboolean genre_match (gchar * string1, gchar * string2)
{
    gchar **genres = g_strsplit (string1, " ", -1);
    gboolean matched = FALSE;
    gint n;

    if (genres != NULL)
    {
        for (n = 0; genres[n] != NULL; n++)
        {
            if (mystrcasestr (string2, genres[n]))
            {
                matched = TRUE;
                break;
            }
        }
        g_strfreev (genres);
    }

    return matched;
}
