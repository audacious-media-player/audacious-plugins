
#ifndef SHOUTCAST_H
#define SHOUTCAST_H

#include "streambrowser.h"
#include "streamdir.h"

// todo: replace hardcoded image paths with DATA_DIR G_DIR_SEPARATOR_S "images" G_DIR_SEPARATOR_S "whatever.png"

#define SHOUTCAST_NAME			"Shoutcast"
#define SHOUTCAST_ICON			"/usr/share/audacious/images/menu_playlist.png"
#define SHOUTCAST_STREAMDIR_URL		"http://www.shoutcast.com/sbin/newxml.phtml"
#define SHOUTCAST_CATEGORY_URL		"http://www.shoutcast.com/sbin/newxml.phtml?genre=%s"
#define SHOUTCAST_STREAMINFO_URL	"http://www.shoutcast.com/sbin/shoutcast-playlist.pls?rn=%s&file=filename.pls"
#define SHOUTCAST_BUFFER_SIZE		256


gboolean			shoutcast_category_fetch(category_t *category);
streamdir_t*			shoutcast_streamdir_fetch();


#endif	// SHOUTCAST_H

