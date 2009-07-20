#include <audacious/plugin.h>

void wv_load_config();
void wv_configure();
void wv_about_box(void);
void wv_play(InputPlayback *data);
void wv_stop(InputPlayback *data);
void wv_pause(InputPlayback *data, gshort pause);
void wv_seek(InputPlayback *data, gint sec);
gint wv_get_time(InputPlayback *data);
void wv_get_song_info(gchar *filename, gchar **title, gint *length);
Tuple *wv_get_song_tuple(const gchar *filename);
gint wv_is_our_fd(const gchar *filename, VFSFile *file);
Tuple *wv_probe_for_tuple(const gchar *filename, VFSFile *file);
void wv_file_info_box(gchar *);

static gchar *wv_fmts[] = { "wv", NULL };

InputPlugin wvpack = {
    .description = (gchar *)"WavPack Audio Plugin",
    .init = wv_load_config,
    .about = wv_about_box,
    .configure = wv_configure,
    .enabled = FALSE,
    .play_file = wv_play,
    .stop = wv_stop,
    .pause = wv_pause,
    .seek = wv_seek,
    .get_time = wv_get_time,
    .get_song_info = wv_get_song_info,
    .file_info_box = wv_file_info_box,
    .get_song_tuple = wv_get_song_tuple,
    .is_our_file_from_vfs = wv_is_our_fd,
    .vfs_extensions = wv_fmts,
    .probe_for_tuple = wv_probe_for_tuple,
};

InputPlugin *wv_iplist[] = { &wvpack, NULL };

DECLARE_PLUGIN(wavpack, NULL, NULL, wv_iplist, NULL, NULL, NULL, NULL,NULL);
