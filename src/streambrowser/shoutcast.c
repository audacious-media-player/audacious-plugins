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
#include <audacious/plugin.h>

#include "streambrowser.h"
#include "shoutcast.h"


gboolean shoutcast_streaminfo_fetch (category_t * category,
                                     streaminfo_t * streaminfo)
{
    gchar url[DEF_STRING_LEN];
    g_snprintf (url, DEF_STRING_LEN, SHOUTCAST_CATEGORY_URL, category->name);

    /* generate a valid, temporary filename */
    char *temp_filename = tempnam (NULL, "aud_sb");
    if (temp_filename == NULL)
    {
        failure ("shoutcast: failed to create a temporary file\n");
        return FALSE;
    }

    char temp_pathname[DEF_STRING_LEN];
    sprintf (temp_pathname, "file://%s", temp_filename);

    AUDDBG("shoutcast: fetching category file '%s'\n", url);
    if (!fetch_remote_to_local_file (url, temp_pathname))
    {
        failure
            ("shoutcast: category file '%s' could not be downloaded to '%s'\n",
             url, temp_pathname);
        free (temp_filename);
        return FALSE;
    }
    AUDDBG("shoutcast: category file '%s' successfuly downloaded to '%s'\n",
           url, temp_pathname);

    xmlDoc *doc = xmlReadFile (temp_pathname, NULL, 0);
    if (doc == NULL)
    {
        failure ("shoutcast: failed to read '%s' category file\n",
                 category->name);
        free (temp_filename);
        return FALSE;
    }

    xmlNode *root_node = xmlDocGetRootElement (doc);
    xmlNode *node;

    root_node = root_node->children;

    for (node = root_node; node != NULL; node = node->next)
    {
        if (node->type == XML_ELEMENT_NODE
            && !strcmp ((char *) node->name, "station"))
        {
            gchar *streaminfo_name =
                (gchar *) xmlGetProp (node, (xmlChar *) "name");
            gchar *streaminfo_id =
                (gchar *) xmlGetProp (node, (xmlChar *) "id");
            gchar streaminfo_playlist_url[DEF_STRING_LEN];
            gchar *streaminfo_current_track =
                (gchar *) xmlGetProp (node, (xmlChar *) "ct");

            g_snprintf (streaminfo_playlist_url, DEF_STRING_LEN,
                        SHOUTCAST_STREAMINFO_URL, streaminfo_id);

            if (strncmp
                (streaminfo_playlist_url, streaminfo->playlist_url,
                 DEF_STRING_LEN) == 0)
            {
                AUDDBG
                    ("shoutcast: updating stream info for '%s' with id %s from '%s'\n",
                     streaminfo_name, streaminfo_id, url);

                strcpy (streaminfo->name, streaminfo_name);
                strcpy (streaminfo->playlist_url, streaminfo_playlist_url);
                strcpy (streaminfo->current_track, streaminfo_current_track);

                xmlFree (streaminfo_name);
                xmlFree (streaminfo_id);
                xmlFree (streaminfo_current_track);

                AUDDBG("shoutcast: stream info added\n");

                break;
            }
        }
    }

    xmlFreeDoc (doc);

    if (remove (temp_filename) != 0)
    {
        failure ("shoutcast: cannot remove the temporary file: %s\n",
                 strerror (errno));
    }
    free (temp_filename);

    return TRUE;
}

gboolean shoutcast_category_fetch (streamdir_t * streamdir,
                                   category_t * category)
{
    gchar url[DEF_STRING_LEN];
    g_snprintf (url, DEF_STRING_LEN, SHOUTCAST_CATEGORY_URL, category->name);

    /* generate a valid, temporary filename */
    char *temp_filename = tempnam (NULL, "aud_sb");
    if (temp_filename == NULL)
    {
        failure ("shoutcast: failed to create a temporary file\n");
        return FALSE;
    }

    char temp_pathname[DEF_STRING_LEN];
    sprintf (temp_pathname, "file://%s", temp_filename);

    AUDDBG("shoutcast: fetching category file '%s'\n", url);
    if (!fetch_remote_to_local_file (url, temp_pathname))
    {
        failure
            ("shoutcast: category file '%s' could not be downloaded to '%s'\n",
             url, temp_pathname);
        free (temp_filename);
        return FALSE;
    }
    AUDDBG("shoutcast: category file '%s' successfuly downloaded to '%s'\n",
           url, temp_pathname);

    xmlDoc *doc = xmlReadFile (temp_pathname, NULL, 0);
    if (doc == NULL)
    {
        failure ("shoutcast: failed to read '%s' category file\n",
                 category->name);
        free (temp_filename);
        return FALSE;
    }

    /* free/remove any existing streaminfos in this category */
    while (streaminfo_get_count (category) > 0)
        streaminfo_remove (category, streaminfo_get_by_index (category, 0));

    xmlNode *root_node = xmlDocGetRootElement (doc);
    xmlNode *node;

    root_node = root_node->children;

    for (node = root_node; node != NULL; node = node->next)
    {
        if (node->type == XML_ELEMENT_NODE
            && !strcmp ((char *) node->name, "station"))
        {
            gchar *streaminfo_name =
                (gchar *) xmlGetProp (node, (xmlChar *) "name");
            gchar *streaminfo_id =
                (gchar *) xmlGetProp (node, (xmlChar *) "id");
            gchar streaminfo_playlist_url[DEF_STRING_LEN];
            gchar *streaminfo_current_track =
                (gchar *) xmlGetProp (node, (xmlChar *) "ct");

            g_snprintf (streaminfo_playlist_url, DEF_STRING_LEN,
                        SHOUTCAST_STREAMINFO_URL, streaminfo_id);

            AUDDBG("shoutcast: adding stream info for '%s/%s' from '%s'\n",
                   streaminfo_name, streaminfo_id, url);

            streaminfo_t *streaminfo =
                streaminfo_new (streaminfo_name, streaminfo_playlist_url, "",
                                streaminfo_current_track);
            streaminfo_add (category, streaminfo);

            xmlFree (streaminfo_name);
            xmlFree (streaminfo_id);
            xmlFree (streaminfo_current_track);

            AUDDBG("shoutcast: stream info added\n");
        }
    }

    xmlFreeDoc (doc);

    if (remove (temp_filename) != 0)
    {
        failure ("shoutcast: cannot remove the temporary file: %s\n",
                 strerror (errno));
    }
    free (temp_filename);

    return TRUE;
}


streamdir_t *shoutcast_streamdir_fetch ()
{
    streamdir_t *streamdir = streamdir_new (SHOUTCAST_NAME);

    /* generate a valid, temporary filename */
    char *temp_filename = tempnam (NULL, "aud_sb");
    if (temp_filename == NULL)
    {
        failure ("shoutcast: failed to create a temporary file\n");
        return NULL;
    }

    char temp_pathname[DEF_STRING_LEN];
    sprintf (temp_pathname, "file://%s", temp_filename);

    AUDDBG("shoutcast: fetching streaming directory file '%s'\n",
           SHOUTCAST_STREAMDIR_URL);
    if (!fetch_remote_to_local_file (SHOUTCAST_STREAMDIR_URL, temp_pathname))
    {
        failure
            ("shoutcast: stream directory file '%s' could not be downloaded to '%s'\n",
             SHOUTCAST_STREAMDIR_URL, temp_pathname);
        free (temp_filename);
        return NULL;
    }
    AUDDBG
        ("shoutcast: stream directory file '%s' successfuly downloaded to '%s'\n",
         SHOUTCAST_STREAMDIR_URL, temp_pathname);

    xmlDoc *doc = xmlReadFile (temp_pathname, NULL, 0);
    if (doc == NULL)
    {
        failure ("shoutcast: failed to read stream directory file\n");
        free (temp_filename);
        return NULL;
    }

    xmlNode *root_node = xmlDocGetRootElement (doc);
    xmlNode *node;

    root_node = root_node->children;

    for (node = root_node; node != NULL; node = node->next)
    {
        if (node->type == XML_ELEMENT_NODE)
        {
            gchar *category_name =
                (gchar *) xmlGetProp (node, (xmlChar *) "name");

            AUDDBG("shoutcast: fetching category '%s'\n", category_name);

            category_t *category = category_new (category_name);
            category_add (streamdir, category);

            xmlFree (category_name);

            AUDDBG("shoutcast: category added: %s.\n", category_name);
        }
    }

    xmlFreeDoc (doc);

    if (remove (temp_filename) != 0)
    {
        failure ("shoutcast: cannot remove the temporary file: %s\n",
                 strerror (errno));
    }
    free (temp_filename);

    AUDDBG("shoutcast: streaming directory successfuly loaded\n");

    return streamdir;
}
