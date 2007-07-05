/*
 * Audacious: A cross-platform multimedia player
 * Copyright (c) 2006 William Pitcock, Tony Vroon, George Averill,
 *                    Giacomo Lozito, Derek Pomery and Yoshiki Yazawa.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include "audacious/main.h"
#include "audacious/util.h"
#include "audacious/playlist.h"
#include "audacious/playlist_container.h"
#include "audacious/plugin.h"

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>

#define XSPF_ROOT_NODE_NAME "playlist"
#define XSPF_XMLNS "http://xspf.org/ns/0/"

#define TMP_BUF_LEN 128

gchar *base = NULL;

static gboolean is_uri(gchar *uri)
{
    if(strstr(uri, "://"))
        return TRUE;
    else
        return FALSE;
}

static gboolean is_remote(gchar *uri)
{
    if(strstr(uri, "file://"))
        return FALSE;

    if(strstr(uri, "://"))
        return TRUE;
    else
        return FALSE;
}

// this function is taken from libxml2-2.6.27.
static xmlChar *audPathToURI(const xmlChar *path)
{
    xmlURIPtr uri;
    xmlURI temp;
    xmlChar *ret, *cal;

    if(path == NULL)
        return NULL;

    if((uri = xmlParseURI((const char *)path)) != NULL) {
        xmlFreeURI(uri);
        return xmlStrdup(path);
    }
    cal = xmlCanonicPath(path);
    if(cal == NULL)
        return NULL;
    memset(&temp, 0, sizeof(temp));
    temp.path = (char *)cal;
    ret = xmlSaveUri(&temp);
    xmlFree(cal);
    return ret;
}

static void add_file(xmlNode *track, const gchar *filename, gint pos)
{
    xmlNode *nptr;
    TitleInput *tuple;
    gchar *location = NULL;
    Playlist *playlist = playlist_get_active();

    tuple = bmp_title_input_new();

    tuple->length = -1;
    tuple->mtime = -1;          // mark as uninitialized.

    // creator, album, title, duration, trackNum, annotation, image, 
    for(nptr = track->children; nptr != NULL; nptr = nptr->next) {
        if(nptr->type == XML_ELEMENT_NODE
           && !xmlStrcmp(nptr->name, (xmlChar *)"location")) {
            gchar *str = (gchar *)xmlNodeGetContent(nptr);
            gchar *tmp;

            if(!is_remote(str)) {   /* local file */
                tmp = (gchar *)xmlURIUnescapeString(str, -1, NULL);
            }
            else {              /* streaming */
                tmp = g_strdup(str);
            }

            location = g_strdup_printf("%s%s", base ? base : "", tmp);

            xmlFree(str);
            g_free(tmp);
        }
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"title")) {
            tuple->track_name = (gchar *)xmlNodeGetContent(nptr);
        }
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"creator")) {
            tuple->performer = (gchar *)xmlNodeGetContent(nptr);
        }
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"annotation")) {
            tuple->comment = (gchar *)xmlNodeGetContent(nptr);
        }
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"album")) {
            tuple->album_name = (gchar *)xmlNodeGetContent(nptr);
        }
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"trackNum")) {
            xmlChar *str = xmlNodeGetContent(nptr);
            tuple->track_number = atol((char *)str);
            xmlFree(str);
        }
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"duration")) {
            xmlChar *str = xmlNodeGetContent(nptr);
            tuple->length = atol((char *)str);
            xmlFree(str);
        }

        //
        // additional metadata
        //
        // year, date, genre, formatter, mtime
        //
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"meta")) {
            xmlChar *rel = NULL;

            rel = xmlGetProp(nptr, (xmlChar *)"rel");

            if(!xmlStrcmp(rel, (xmlChar *)"year")) {
                xmlChar *cont = xmlNodeGetContent(nptr);
                tuple->year = atol((char *)cont);
                xmlFree(cont);
                continue;
            }
            else if(!xmlStrcmp(rel, (xmlChar *)"date")) {
                tuple->date = (gchar *)xmlNodeGetContent(nptr);
                continue;
            }
            else if(!xmlStrcmp(rel, (xmlChar *)"genre")) {
                tuple->genre = (gchar *)xmlNodeGetContent(nptr);
                continue;
            }
            else if(!xmlStrcmp(rel, (xmlChar *)"formatter")) {
                tuple->formatter = (gchar *)xmlNodeGetContent(nptr);
                continue;
            }
            else if(!xmlStrcmp(rel, (xmlChar *)"mtime")) {
                xmlChar *str = NULL;
                str = xmlNodeGetContent(nptr);
                tuple->mtime = (time_t) atoll((char *)str);
                xmlFree(str);
                continue;
            }
            xmlFree(rel);
            rel = NULL;
        }

    }

    if(location) {
        tuple->file_name = g_path_get_basename(location);
        tuple->file_path = g_path_get_dirname(location);
        tuple->file_ext = g_strdup(strrchr(location, '.'));
        // add file to playlist
        playlist_load_ins_file_tuple(playlist, location, filename, pos, tuple);
        pos++;
    }

    g_free(location);
    location = NULL;
}

static void find_track(xmlNode *tracklist, const gchar *filename, gint pos)
{
    xmlNode *nptr;
    for(nptr = tracklist->children; nptr != NULL; nptr = nptr->next) {
        if(nptr->type == XML_ELEMENT_NODE
           && !xmlStrcmp(nptr->name, (xmlChar *)"track")) {
            add_file(nptr, filename, pos);
        }
    }
}

static void find_audoptions(xmlNode *tracklist, const gchar *filename, gint pos)
{
    xmlNode *nptr;
    Playlist *playlist = playlist_get_active();

    for(nptr = tracklist->children; nptr != NULL; nptr = nptr->next) {
        if(nptr->type == XML_ELEMENT_NODE
           && !xmlStrcmp(nptr->name, (xmlChar *)"options")) {
            xmlChar *opt = NULL;

            opt = xmlGetProp(nptr, (xmlChar *)"staticlist");
            if(!strcasecmp((char *)opt, "true")) {
                playlist->attribute |= PLAYLIST_STATIC;
            }
            else
                playlist->attribute ^= PLAYLIST_STATIC;
            xmlFree(opt);
            opt = NULL;
        }
    }
}

static void playlist_load_xspf(const gchar *filename, gint pos)
{
    xmlDocPtr doc;
    xmlNode *nptr, *nptr2;

    g_return_if_fail(filename != NULL);

    doc = xmlParseFile(filename);
    if(doc == NULL)
        return;

    xmlFree(base);
    base = NULL;

    // find trackList
    for(nptr = doc->children; nptr != NULL; nptr = nptr->next) {
        if(nptr->type == XML_ELEMENT_NODE
           && !xmlStrcmp(nptr->name, (xmlChar *)"playlist")) {
            base = (gchar *)xmlNodeGetBase(doc, nptr);
#ifdef DEBUG
            printf("load: base = %s\n", base);
#endif
            {
                gchar *tmp = xmlURIUnescapeString(base, -1, NULL);
                if(tmp) {
                    g_free(base);
                    base = tmp;
                }
            }

            if(!strcmp(base, filename)) {   // filename is specified as a base URI. ignore.
                xmlFree(base);
                base = NULL;
            }

            for(nptr2 = nptr->children; nptr2 != NULL; nptr2 = nptr2->next) {

                if(nptr2->type == XML_ELEMENT_NODE
                   && !xmlStrcmp(nptr2->name, (xmlChar *)"extension")) {
                    //check if application is audacious
                    xmlChar *app = NULL;
                    app = xmlGetProp(nptr2, (xmlChar *)"application");
                    if(!xmlStrcmp(app, (xmlChar *)"audacious")) {
                        find_audoptions(nptr2, filename, pos);
                    }
                    xmlFree(app);
                }

                if(nptr2->type == XML_ELEMENT_NODE
                   && !xmlStrcmp(nptr2->name, (xmlChar *)"trackList")) {
                    find_track(nptr2, filename, pos);
                }
            }
        }
    }
    xmlFreeDoc(doc);
}

static void playlist_save_xspf(const gchar *filename, gint pos)
{
    xmlDocPtr doc;
    xmlNodePtr rootnode, tmp, tracklist;
    GList *node;
    gint baselen = 0;
    Playlist *playlist = playlist_get_active();

    xmlFree(base);
    base = NULL;

    doc = xmlNewDoc((xmlChar *)"1.0");

    doc->charset = XML_CHAR_ENCODING_UTF8;
    doc->encoding = xmlStrdup((xmlChar *)"UTF-8");

    rootnode = xmlNewNode(NULL, (xmlChar *)XSPF_ROOT_NODE_NAME);
    xmlSetProp(rootnode, (xmlChar *)"version", (xmlChar *)"1");
    xmlSetProp(rootnode, (xmlChar *)"xmlns", (xmlChar *)XSPF_XMLNS);

    PLAYLIST_LOCK(playlist->mutex);
    if(playlist->attribute & PLAYLIST_USE_RELATIVE) {
        /* prescan to determine base uri */
        for(node = playlist->entries; node != NULL; node = g_list_next(node)) {
            gchar *ptr1, *ptr2;
            PlaylistEntry *entry = PLAYLIST_ENTRY(node->data);
            gchar *tmp;
            gint tmplen = 0;

            if(!is_uri(entry->filename)) {
                gchar *tmp2;
                tmp2 = g_path_get_dirname(entry->filename);
                tmp = g_strdup_printf("%s/", tmp2);
                g_free(tmp2);
                tmp2 = NULL;
            }
            else {
                tmp = g_strdup(entry->filename);
            }

            if(!base) {
                base = strdup(tmp);
                baselen = strlen(base);
            }
            ptr1 = base;
            ptr2 = tmp;

            while(ptr1 && ptr2 && *ptr1 && *ptr2 && *ptr1 == *ptr2) {
                ptr1++;
                ptr2++;
            }
            *ptr2 = '\0';       //terminate
            tmplen = ptr2 - tmp;

            if(tmplen <= baselen) {
                g_free(base);
                base = tmp;
                baselen = tmplen;
#ifdef DEBUG
                printf("base = \"%s\" baselen = %d\n", base, baselen);
#endif
            }
            else {
                g_free(tmp);
                tmp = NULL;
            }
        }
        /* set base URI */
        if(base) {
            gchar *tmp;
            if(!is_uri(base)) {
                tmp = (gchar *)audPathToURI((xmlChar *)base);
                if(tmp) {
                    g_free(base);
                    base = tmp;
                }
            }
//            xmlNodeSetBase(rootnode, base); // it blindly escapes characters.

            if(!is_uri(base)) {
                tmp = g_strdup_printf("file://%s", base);
                xmlSetProp(rootnode, (xmlChar *)"xml:base", (xmlChar *)tmp);
                g_free(tmp);
                tmp = NULL;
            }
            else
                xmlSetProp(rootnode, (xmlChar *)"xml:base", (xmlChar *)base);
        }
    }                           /* USE_RELATIVE */


    xmlDocSetRootElement(doc, rootnode);

    tmp = xmlNewNode(NULL, (xmlChar *)"creator");
    xmlAddChild(tmp, xmlNewText((xmlChar *)PACKAGE "-" VERSION));
    xmlAddChild(rootnode, tmp);

    // add staticlist marker
    if(playlist->attribute & PLAYLIST_STATIC) {
        xmlNodePtr extension, options;

        extension = xmlNewNode(NULL, (xmlChar *)"extension");
        xmlSetProp(extension, (xmlChar *)"application", (xmlChar *)"audacious");

        options = xmlNewNode(NULL, (xmlChar *)"options");
        xmlSetProp(options, (xmlChar *)"staticlist", (xmlChar *)"true");

        xmlAddChild(extension, options);
        xmlAddChild(rootnode, extension);
    }

    tracklist = xmlNewNode(NULL, (xmlChar *)"trackList");
    xmlAddChild(rootnode, tracklist);

//  PLAYLIST_LOCK(playlist->mutex);

    for(node = playlist->entries; node != NULL; node = g_list_next(node)) {
        PlaylistEntry *entry = PLAYLIST_ENTRY(node->data);
        xmlNodePtr track, location;
        gchar *filename = NULL;

        track = xmlNewNode(NULL, (xmlChar *)"track");
        location = xmlNewNode(NULL, (xmlChar *)"location");

        if(is_uri(entry->filename)) {   /* remote uri */
            gchar *tmp = NULL;
#ifdef DEBUG
            printf("filename is uri\n");
#endif
            tmp = (gchar *)xmlURIEscape((xmlChar *)entry->filename);
            filename = g_strdup(entry->filename + baselen);
            g_free(tmp);
            tmp = NULL;
        }
        else {                  /* local file */
            gchar *tmp =
                (gchar *)audPathToURI((const xmlChar *)entry->filename + baselen);
            if(base) {
                filename = g_strdup_printf("%s", tmp);
            }
            else {
#ifdef DEBUG
                printf("absolule, local\n");
#endif
                filename = g_strdup_printf("file://%s", tmp);
            }
            g_free(tmp);
            tmp = NULL;
        }

        if(!g_utf8_validate(filename, -1, NULL))
            continue;

        xmlAddChild(location, xmlNewText((xmlChar *)filename));
        xmlAddChild(track, location);
        xmlAddChild(tracklist, track);

        /* do we have a tuple? */
        if(entry->tuple != NULL) {

            if(entry->tuple->track_name != NULL &&
               g_utf8_validate(entry->tuple->track_name, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"title");
                xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->track_name));
                xmlAddChild(track, tmp);
            }

            if(entry->tuple->performer != NULL &&
               g_utf8_validate(entry->tuple->performer, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"creator");
                xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->performer));
                xmlAddChild(track, tmp);
            }

            if(entry->tuple->comment != NULL &&
               g_utf8_validate(entry->tuple->comment, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"annotation");
                xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->comment));
                xmlAddChild(track, tmp);
            }

            if(entry->tuple->album_name != NULL &&
               g_utf8_validate(entry->tuple->album_name, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"album");
                xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->album_name));
                xmlAddChild(track, tmp);
            }

            if(entry->tuple->track_number != 0) {
                gchar *str;
                str = g_malloc(TMP_BUF_LEN);
                tmp = xmlNewNode(NULL, (xmlChar *)"trackNum");
                sprintf(str, "%d", entry->tuple->track_number);
                xmlAddChild(tmp, xmlNewText((xmlChar *)str));
                g_free(str);
                str = NULL;
                xmlAddChild(track, tmp);
            }

            if(entry->tuple->length > 0) {
                gchar *str;
                str = g_malloc(TMP_BUF_LEN);
                tmp = xmlNewNode(NULL, (xmlChar *)"duration");
                sprintf(str, "%d", entry->tuple->length);
                xmlAddChild(tmp, xmlNewText((xmlChar *)str));
                g_free(str);
                str = NULL;
                xmlAddChild(track, tmp);
            }

            //
            // additional metadata
            //
            // year, date, genre, formatter, mtime
            //

            if(entry->tuple->year != 0) {
                gchar *str;
                str = g_malloc(TMP_BUF_LEN);
                tmp = xmlNewNode(NULL, (xmlChar *)"meta");
                xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"year");
                sprintf(str, "%d", entry->tuple->year);
                xmlAddChild(tmp, xmlNewText((xmlChar *)str));
                xmlAddChild(track, tmp);
                g_free(str);
                str = NULL;
            }

            if(entry->tuple->date != NULL &&
               g_utf8_validate(entry->tuple->date, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"meta");
                xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"date");
                xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->date));
                xmlAddChild(track, tmp);
            }

            if(entry->tuple->genre != NULL &&
               g_utf8_validate(entry->tuple->genre, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"meta");
                xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"genre");
                xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->genre));
                xmlAddChild(track, tmp);
            }

            if(entry->tuple->formatter != NULL &&
               g_utf8_validate(entry->tuple->formatter, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"meta");
                xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"formatter");
                xmlAddChild(tmp, xmlNewText((xmlChar *)entry->tuple->formatter));
                xmlAddChild(track, tmp);
            }

            // mtime: write mtime unconditionally to support staticlist.
            {
                gchar *str = g_malloc(TMP_BUF_LEN);
                tmp = xmlNewNode(NULL, (xmlChar *)"meta");
                xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"mtime");
                sprintf(str, "%ld", (long)entry->tuple->mtime);

                xmlAddChild(tmp, xmlNewText((xmlChar *)str));
                xmlAddChild(track, tmp);
                g_free(str);
                str = NULL;
            }

        }                       /* tuple */
        else {

            if(entry->title != NULL && g_utf8_validate(entry->title, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"title");
                xmlAddChild(tmp, xmlNewText((xmlChar *)entry->title));
                xmlAddChild(track, tmp);
            }

            if(entry->length > 0) {
                gchar *str;
                str = g_malloc(TMP_BUF_LEN);
                tmp = xmlNewNode(NULL, (xmlChar *)"duration");
                sprintf(str, "%d", entry->length);
                xmlAddChild(tmp, xmlNewText((xmlChar *)str));
                g_free(str);
                str = NULL;
                xmlAddChild(track, tmp);
            }

            /* add mtime of -1 */
            {
                gchar *str = g_malloc(TMP_BUF_LEN);
                tmp = xmlNewNode(NULL, (xmlChar *)"meta");
                xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"mtime");
                sprintf(str, "%ld", -1L);

                xmlAddChild(tmp, xmlNewText((xmlChar *)str));
                xmlAddChild(track, tmp);
                g_free(str);
                str = NULL;
            }

        }                       /* no tuple */

        g_free(filename);
        filename = NULL;
    }

    PLAYLIST_UNLOCK(playlist->mutex);

    xmlSaveFormatFile(filename, doc, 1);
    xmlFreeDoc(doc);
    doc = NULL;

    xmlFree(base);
    base = NULL;
}

PlaylistContainer plc_xspf = {
    .name = "XSPF Playlist Format",
    .ext = "xspf",
    .plc_read = playlist_load_xspf,
    .plc_write = playlist_save_xspf,
};

static void init(void)
{
    playlist_container_register(&plc_xspf);
}

static void cleanup(void)
{
    playlist_container_unregister(&plc_xspf);
}

DECLARE_PLUGIN(xspf, init, cleanup, NULL, NULL, NULL, NULL, NULL);
