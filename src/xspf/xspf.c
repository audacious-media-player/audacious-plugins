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

#if 0
static gboolean is_remote(gchar *uri)
{
    if(strstr(uri, "file://"))
        return FALSE;

    if(strstr(uri, "://"))
        return TRUE;
    else
        return FALSE;
}
#endif

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
    Tuple *tuple;
    gchar *location = NULL;
    Playlist *playlist = playlist_get_active();

    tuple = tuple_new();

    tuple_associate_int(tuple, "length", -1);
    tuple_associate_int(tuple, "mtime", -1);          // mark as uninitialized.

    // creator, album, title, duration, trackNum, annotation, image, 
    for(nptr = track->children; nptr != NULL; nptr = nptr->next) {
        if(nptr->type == XML_ELEMENT_NODE
           && !xmlStrcmp(nptr->name, (xmlChar *)"location")) {
            gchar *str = (gchar *)xmlNodeGetContent(nptr);
            gchar *tmp = NULL;

            // tmp is escaped uri or a part of escaped uri.
            tmp = g_strdup_printf("%s%s", base ? base : "", str);
            location = g_filename_from_uri(tmp, NULL, NULL);
            if(!location) // http:// or something.
                location = g_strdup(tmp);

            xmlFree(str); str = NULL;
            g_free(tmp); tmp = NULL;
        }
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"title")) {
            xmlChar *str = xmlNodeGetContent(nptr);
	    tuple_associate_string(tuple, "title", (gchar *) str);
            xmlFree(str);
        }
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"creator")) {
            xmlChar *str = xmlNodeGetContent(nptr);
	    tuple_associate_string(tuple, "artist", (gchar *) str);
            xmlFree(str);
        }
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"annotation")) {
            xmlChar *str = xmlNodeGetContent(nptr);
	    tuple_associate_string(tuple, "comment", (gchar *) str);
            xmlFree(str);
        }
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"album")) {
            xmlChar *str = xmlNodeGetContent(nptr);
	    tuple_associate_string(tuple, "album", (gchar *) str);
            xmlFree(str);
        }
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"trackNum")) {
            xmlChar *str = xmlNodeGetContent(nptr);
            tuple_associate_int(tuple, "track-number", atol((char *)str));
            xmlFree(str);
        }
        else if(nptr->type == XML_ELEMENT_NODE
                && !xmlStrcmp(nptr->name, (xmlChar *)"duration")) {
            xmlChar *str = xmlNodeGetContent(nptr);
            tuple_associate_int(tuple, "length", atol((char *)str));
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
                tuple_associate_int(tuple, "year", atol((char *)cont));
                xmlFree(cont);
                continue;
            }
            else if(!xmlStrcmp(rel, (xmlChar *)"date")) {
                xmlChar *cont = xmlNodeGetContent(nptr);
                tuple_associate_string(tuple, "date", (gchar *) cont);
                xmlFree(cont);
                continue;
            }
            else if(!xmlStrcmp(rel, (xmlChar *)"genre")) {
                xmlChar *cont = xmlNodeGetContent(nptr);
                tuple_associate_string(tuple, "genre", (gchar *) cont);
                xmlFree(cont);
                continue;
            }
            else if(!xmlStrcmp(rel, (xmlChar *)"formatter")) {
                xmlChar *cont = xmlNodeGetContent(nptr);
                tuple_associate_string(tuple, "formatter", (gchar *) cont);
                xmlFree(cont);
                continue;
            }
            else if(!xmlStrcmp(rel, (xmlChar *)"mtime")) {
                xmlChar *str = NULL;
                str = xmlNodeGetContent(nptr);
                tuple_associate_int(tuple, "mtime", atoll((char *)str));
                xmlFree(str);
                continue;
            }
            xmlFree(rel);
            rel = NULL;
        }

    }

    if(location) {
        gchar *uri = NULL;
        gchar *scratch;

        scratch = g_path_get_basename(location);
        tuple_associate_string(tuple, "file-name", scratch);
        g_free(scratch);

        scratch = g_path_get_dirname(location);
        tuple_associate_string(tuple, "file-path", scratch);
        g_free(scratch);

#ifdef DEBUG
        printf("xspf: tuple->file_name = %s\n", tuple_get_string(tuple, "file-name"));
        printf("xspf: tuple->file_path = %s\n", tuple_get_string(tuple, "file-path"));
#endif
        tuple_associate_string(tuple, "file-ext", strrchr(location, '.'));
        // add file to playlist
        uri = g_filename_to_uri(location, NULL, NULL);
        // uri would be NULL if location is already uri. --yaz
        playlist_load_ins_file_tuple(playlist, uri ? uri: location, filename, pos, tuple);
        g_free(uri); uri = NULL;
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
    gchar *tmp = NULL;

    g_return_if_fail(filename != NULL);
#ifdef DEBUG
    printf("playlist_load_xspf: filename = %s\n", filename);
#endif
    doc = xmlRecoverFile(filename);
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
            printf("playlist_load_xspf: base @1 = %s\n", base);
#endif
            // if filename is specified as a base, ignore it.
            tmp = xmlURIUnescapeString(base, -1, NULL);
            if(tmp) {
                if(!strcmp(tmp, filename)) {   
                    xmlFree(base);
                    base = NULL;
                }
                g_free(tmp);
                tmp = NULL;
            }
#ifdef DEBUG
            printf("playlist_load_xspf: base @2 = %s\n", base);
#endif
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

#ifdef DEBUG
    printf("playlist_save_xspf: filename = %s\n", filename);
#endif
    xmlFree(base);
    base = NULL;

    doc = xmlNewDoc((xmlChar *)"1.0");

    doc->charset = XML_CHAR_ENCODING_UTF8;
    doc->encoding = xmlStrdup((xmlChar *)"UTF-8");

    rootnode = xmlNewNode(NULL, (xmlChar *)XSPF_ROOT_NODE_NAME);
    xmlSetProp(rootnode, (xmlChar *)"version", (xmlChar *)"1");
    xmlSetProp(rootnode, (xmlChar *)"xmlns", (xmlChar *)XSPF_XMLNS);

    PLAYLIST_LOCK(playlist->mutex);

    /* relative */
    if(playlist->attribute & PLAYLIST_USE_RELATIVE) {
        /* prescan to determine base uri */
        for(node = playlist->entries; node != NULL; node = g_list_next(node)) {
            gchar *ptr1, *ptr2;
            PlaylistEntry *entry = PLAYLIST_ENTRY(node->data);
            gchar *tmp;
            gint tmplen = 0;

            if(!is_uri(entry->filename)) { //obsolete
                gchar *tmp2;
                tmp2 = g_path_get_dirname(entry->filename);
                tmp = g_strdup_printf("%s/", tmp2);
                g_free(tmp2); tmp2 = NULL;
            }
            else { //uri
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

            if(!is_uri(base)) {
#ifdef DEBUG
                printf("base is not uri. something is wrong.\n");
#endif
                tmp = g_strdup_printf("file://%s", base);
                xmlSetProp(rootnode, (xmlChar *)"xml:base", (xmlChar *)tmp);
                g_free(tmp);
                tmp = NULL;
            }
            else
                xmlSetProp(rootnode, (xmlChar *)"xml:base", (xmlChar *)base);
        }
    }                           /* USE_RELATIVE */

    /* common */
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

    for(node = playlist->entries; node != NULL; node = g_list_next(node)) {
        PlaylistEntry *entry = PLAYLIST_ENTRY(node->data);
        xmlNodePtr track, location;
        gchar *filename = NULL;

        track = xmlNewNode(NULL, (xmlChar *)"track");
        location = xmlNewNode(NULL, (xmlChar *)"location");

        if(is_uri(entry->filename)) {   /* uri */
#ifdef DEBUG
            printf("filename is uri\n");
#endif
            filename = g_strdup(entry->filename + baselen); // entry->filename is always uri now.
        }
        else {                  /* local file (obsolete) */
            gchar *tmp =
                (gchar *)audPathToURI((const xmlChar *)entry->filename + baselen);
            if(base) { /* relative */
                filename = g_strdup_printf("%s", tmp);
            }
            else {
#ifdef DEBUG
                printf("absolute and local (obsolete)\n");
#endif
                filename = g_filename_to_uri(tmp, NULL, NULL);
            }
            g_free(tmp); tmp = NULL;
        } /* obsolete */

        if(!g_utf8_validate(filename, -1, NULL))
            continue;

        xmlAddChild(location, xmlNewText((xmlChar *)filename));
        xmlAddChild(track, location);
        xmlAddChild(tracklist, track);

        /* do we have a tuple? */
        if(entry->tuple != NULL) {
            const gchar *scratch;

            if((scratch = tuple_get_string(entry->tuple, "title")) != NULL &&
               g_utf8_validate(scratch, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"title");
                xmlAddChild(tmp, xmlNewText((xmlChar *) scratch));
                xmlAddChild(track, tmp);
            }

            if((scratch = tuple_get_string(entry->tuple, "artist")) != NULL &&
               g_utf8_validate(scratch, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"creator");
                xmlAddChild(tmp, xmlNewText((xmlChar *) scratch));
                xmlAddChild(track, tmp);
            }

            if((scratch = tuple_get_string(entry->tuple, "comment")) != NULL &&
               g_utf8_validate(scratch, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"annotation");
                xmlAddChild(tmp, xmlNewText((xmlChar *) scratch));
                xmlAddChild(track, tmp);
            }

            if((scratch = tuple_get_string(entry->tuple, "album")) != NULL &&
               g_utf8_validate(scratch, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"album");
                xmlAddChild(tmp, xmlNewText((xmlChar *) scratch));
                xmlAddChild(track, tmp);
            }

            if(tuple_get_int(entry->tuple, "track-number") != 0) {
                gchar *str;
                str = g_malloc(TMP_BUF_LEN);
                tmp = xmlNewNode(NULL, (xmlChar *)"trackNum");
                sprintf(str, "%d", tuple_get_int(entry->tuple, "track-number"));
                xmlAddChild(tmp, xmlNewText((xmlChar *)str));
                g_free(str);
                str = NULL;
                xmlAddChild(track, tmp);
            }

            if(tuple_get_int(entry->tuple, "length") > 0) {
                gchar *str;
                str = g_malloc(TMP_BUF_LEN);
                tmp = xmlNewNode(NULL, (xmlChar *)"duration");
                sprintf(str, "%d", tuple_get_int(entry->tuple, "length"));
                xmlAddChild(tmp, xmlNewText((xmlChar *) str));
                g_free(str);
                str = NULL;
                xmlAddChild(track, tmp);
            }

            //
            // additional metadata
            //
            // year, date, genre, formatter, mtime
            //

            if(tuple_get_int(entry->tuple, "year") != 0) {
                gchar *str;
                str = g_malloc(TMP_BUF_LEN);
                tmp = xmlNewNode(NULL, (xmlChar *)"meta");
                xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"year");
                sprintf(str, "%d", tuple_get_int(entry->tuple, "year"));
                xmlAddChild(tmp, xmlNewText((xmlChar *)str));
                xmlAddChild(track, tmp);
                g_free(str);
                str = NULL;
            }

            if((scratch = tuple_get_string(entry->tuple, "date")) != NULL &&
               g_utf8_validate(scratch, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"meta");
                xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"date");
                xmlAddChild(tmp, xmlNewText((xmlChar *) scratch));
                xmlAddChild(track, tmp);
            }

            if((scratch = tuple_get_string(entry->tuple, "genre")) != NULL &&
               g_utf8_validate(scratch, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"meta");
                xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"genre");
                xmlAddChild(tmp, xmlNewText((xmlChar *) scratch));
                xmlAddChild(track, tmp);
            }

            if((scratch = tuple_get_string(entry->tuple, "formatter")) != NULL &&
               g_utf8_validate(scratch, -1, NULL)) {
                tmp = xmlNewNode(NULL, (xmlChar *)"meta");
                xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"formatter");
                xmlAddChild(tmp, xmlNewText((xmlChar *) scratch));
                xmlAddChild(track, tmp);
            }

            // mtime: write mtime unconditionally to support staticlist.
            {
                gchar *str = g_malloc(TMP_BUF_LEN);
                tmp = xmlNewNode(NULL, (xmlChar *)"meta");
                xmlSetProp(tmp, (xmlChar *)"rel", (xmlChar *)"mtime");
                sprintf(str, "%ld", (long) tuple_get_int(entry->tuple, "mtime"));

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

DECLARE_PLUGIN(xspf, init, cleanup, NULL, NULL, NULL, NULL, NULL, NULL);
