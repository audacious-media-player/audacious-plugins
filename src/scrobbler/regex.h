#ifndef REGEX_H
#define REGEX_H 1

#include <glib.h>

GSList * regex_str_to_items(gchar* regexps);
void regex_free_list(GSList *list);

gchar * regex_escape(const gchar* data);
gchar * regex_unescape(const gchar* data);

typedef struct {
    gchar* title;
    gchar* artist;
    gchar* album;
} regex_item_t;

regex_item_t* regex_match(const gchar* subject, GSList* rx_list);
regex_item_t* regex_match_parts(const gchar* artist, const gchar* title, const gchar* album, GSList* rx_list);
void regex_match_free(regex_item_t* item);

#endif
