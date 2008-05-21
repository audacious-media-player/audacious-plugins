
#ifndef STREAMDIR_H
#define STREAMDIR_H


#include <glib.h>

#include "streambrowser.h"


typedef struct {

	gchar			name[DEF_STRING_LEN];
	gchar			url[DEF_STRING_LEN];

} streaminfo_t;

typedef struct {

	gchar			name[DEF_STRING_LEN];
	GList*			streaminfo_list;

} category_t;

typedef struct {

	gchar			name[DEF_STRING_LEN];
	GList*			category_list;

} streamdir_t;


streamdir_t*			streamdir_new(gchar *name);
void				streamdir_delete(streamdir_t *streamdir);

category_t*			category_new(gchar *name);
void				category_delete(category_t *category);
void				category_add(streamdir_t *streamdir, category_t *category);
void				category_remove(streamdir_t *streamdir, category_t *category);
category_t*			category_get_by_index(streamdir_t *streamdir, gint index);
category_t*			category_get_by_name(streamdir_t *streamdir, gchar *name);
gint				category_get_count(streamdir_t *streamdir);

streaminfo_t*			streaminfo_new(gchar *name, gchar *url);
void				streaminfo_delete(streaminfo_t *streaminfo);
void				streaminfo_free(streaminfo_t *streaminfo);
void				streaminfo_add(category_t *category, streaminfo_t *streaminfo);
void				streaminfo_remove(category_t *category, streaminfo_t *streaminfo);
streaminfo_t*			streaminfo_get_by_index(category_t *category, gint index);
streaminfo_t*			streaminfo_get_by_name(category_t *category, gchar *name);
gint				streaminfo_get_count(category_t *category);


#endif	// STREAMDIR_H

