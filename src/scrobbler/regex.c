/*
 * regex.c
 *
 *  Created on: 29-07-2012
 *      Author: Arkadiusz Dzięgiel
 */

#include <string.h>
#include <audacious/debug.h>
#include <glib/gprintf.h>

#include "regex.h"
#include "fmt.h"

/**
 * Converts string to GSList of GRegex.
 * Will modify regexps parameter.
 * You can easily free list with regex_free_list.
 */
GSList * regex_str_to_items(gchar* regexps){
    GSList* rx_list = NULL;
    GRegex* rx;

    char *tmp=regexps,*p=regexps;
    while(tmp!=NULL){
        tmp = strstr(tmp, "\n");
        if(tmp!=NULL) {tmp[0] = 0;tmp++;}

        if(p[0]!=0){
            rx=g_regex_new(p,0,0,NULL);
            if(rx != NULL){
                AUDDBG("compiled pattern '%s'\n", p);
                if(rx_list == NULL){
                    rx_list = g_slist_alloc();
                    rx_list->data = rx;
                } else rx_list = g_slist_prepend(rx_list, rx);
            } else {
                AUDDBG("failed to compile pattern '%s'\n", p);
            }
        }

        p=tmp;
    }

    return g_slist_reverse(rx_list);
}

void regex_free_list(GSList *list){
    g_slist_free_full(list, (GDestroyNotify) g_regex_unref);
}

/**
 * Returns newly allocated escaped string.
 */
gchar * regex_escape(const gchar* data){
    return fmt_escape(data);
}

/**
 * Returns newly allocated unescaped string.
 */
gchar * regex_unescape(const gchar* data){
    return fmt_unescape((char*) data);
}

regex_item_t* regex_match(const gchar* subject, GSList* rx_list){
    GSList *l = rx_list;
    regex_item_t* ret = NULL;

    while(l != NULL){
		GMatchInfo *info;
		GRegex *rx = l->data;

        if(g_regex_match(rx, subject, 0 ,&info)){
            ret = g_malloc(sizeof(regex_item_t));

            ret->artist = g_match_info_fetch_named(info, "artist");
            ret->title = g_match_info_fetch_named(info, "title");
            ret->album = g_match_info_fetch_named(info, "album");
        }

        g_match_info_free(info);
        if (ret!=NULL) break;

        l = g_slist_next(l);
    }

    return ret;
}

void regex_match_free(regex_item_t* item){
    if(item == NULL) return;

    g_free(item->artist);
    g_free(item->title);
    g_free(item->album);
    g_free(item);
}

regex_item_t* regex_match_parts(const gchar* artist, const gchar* title, const gchar* album, GSList* rx_list){

    if(!artist) artist="";
    if(!title) title="";
    if(!album) album="";

    gchar buff[strlen(artist)+strlen(title)+strlen(album)+1];
    g_sprintf(buff, "%s—%s—%s", artist, title, album);

    return regex_match(buff, rx_list);
}
