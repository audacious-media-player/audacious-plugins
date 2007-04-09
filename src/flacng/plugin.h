#ifndef _PLUGIN_H
#define _PLUGIN_H

void flac_init(void);
void flac_aboutbox(void);
gboolean flac_is_our_file(gchar* filename);
void flac_play_file (InputPlayback* input);
void flac_stop(InputPlayback* input);
void flac_pause(InputPlayback* input, gshort p);
void flac_seek(InputPlayback* input, gint time);
void flac_get_song_info(gchar* filename, gchar** title, gint* length);
TitleInput *flac_get_song_tuple(gchar* filename);

#endif
