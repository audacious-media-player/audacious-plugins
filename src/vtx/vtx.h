#ifndef VTX_H
#define VTX_H

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <audacious/plugin.h>

void vtx_init(void);
void vtx_about(void);
void vtx_config(void);
void vtx_file_info(char *filename);

extern int  vtx_is_our_file(char *filename);
extern void vtx_play_file (InputPlayback *playback);
extern void vtx_stop (InputPlayback *playback);
extern void vtx_seek (InputPlayback *playback, int time);
extern void vtx_pause (InputPlayback *playback, short p);
extern int  vtx_get_time (InputPlayback *playback);
extern void vtx_get_song_info (char *filename, char **title, int *length);

#endif
