
#ifndef STREAMBROWSER_WIN_H
#define STREAMBROWSER_WIN_H

#include "../streamdir.h"


GtkWidget *		streambrowser_win_init();
void			streambrowser_win_done();
void			streambrowser_win_show();
void			streambrowser_win_hide();

void			streambrowser_win_set_streamdir(streamdir_t *streamdir, gchar *icon_filename);
void			streambrowser_win_set_category(streamdir_t *streamdir, category_t *category);
void			streambrowser_win_set_streaminfo(streamdir_t *streamdir, category_t *category, streaminfo_t *streaminfo);

void			streambrowser_win_set_update_function(void (* update_function) (streamdir_t *streamdir, category_t *category, streaminfo_t *streaminfo, gboolean add_to_playlist));
void			streambrowser_win_set_category_state(streamdir_t *streamdir, category_t *category, gboolean fetching);
void			streambrowser_win_set_streaminfo_state(streamdir_t *streamdir, category_t *category, streaminfo_t *streaminfo, gboolean fetching);


#endif	// STREAMBROWSER_WIN_H

