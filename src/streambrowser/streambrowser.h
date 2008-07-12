
#ifndef STREAMBROWSER_H
#define STREAMBROWSER_H

#include <glib.h>

#include <config.h>
#include <audacious/i18n.h>

#define DEF_STRING_LEN				1024
#define DEF_BUFFER_SIZE				512
#define MAX_UPDATE_THREADS			4
#define PLAYLIST_TEMP_FILE			"file:///tmp/playlist.pls"
#define STREAMBROWSER_ICON_SMALL	DATA_DIR G_DIR_SEPARATOR_S "images" G_DIR_SEPARATOR_S "streambrowser-16x16.png"
#define STREAMBROWSER_ICON			DATA_DIR G_DIR_SEPARATOR_S "images" G_DIR_SEPARATOR_S "streambrowser-64x64.png"


typedef struct {
	
	gboolean		debug;

} streambrowser_cfg_t;

extern streambrowser_cfg_t	streambrowser_cfg;


void				debug(const char *fmt, ...);
void				failure(const char *fmt, ...);
gboolean			fetch_remote_to_local_file(gchar *remote_url, gchar *local_url);


#endif	// STREAMBROWSER_H

