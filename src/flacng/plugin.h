#ifndef _PLUGIN_H
#define _PLUGIN_H

static void flac_init(void);
static void flac_cleanup(void);
static void flac_aboutbox(void);
static gboolean flac_is_our_fd(const gchar* filename, VFSFile* fd);
static void flac_play_file (InputPlayback* input);
static void flac_stop(InputPlayback* input);
static void flac_pause(InputPlayback* input, gshort p);
static void flac_seek(InputPlayback* input, gint time);
static Tuple *flac_probe_for_tuple(const gchar *filename, VFSFile *fd);

#endif
