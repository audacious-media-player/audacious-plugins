#ifndef _CUE_H_
#define _CUE_H_

#include "config.h"

/* #define AUD_DEBUG 1 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <audacious/plugin.h>
#include <audacious/output.h>

#define MAX_CUE_LINE_LENGTH 1000
#define MAX_CUE_TRACKS 1000

extern GMutex *cue_mutex;
extern GCond *cue_cond;

typedef enum {
    STOP = 0,
    RUN,
    EXIT
} watchdog_state_t;

extern watchdog_state_t watchdog_state;

extern gint last_cue_track;
extern gint cur_cue_track;
extern gulong target_time;
extern GMutex *cue_target_time_mutex;

typedef struct cue_tracks {
    gchar *title;
    gchar *performer;
    gint index;
    gint index00;
    gint duration;
} cue_tracks_t;

extern cue_tracks_t cue_tracks[];
extern gint finetune_seek;
extern InputPlayback *real_ip;
extern InputPlayback *caller_ip;

/* prototypes */
void cache_cue_file(const gchar *f);
void free_cue_info(void);
void fix_cue_argument(char *line);
gint is_our_file(const gchar *filename);
void play(InputPlayback *data);
void play_cue_uri(InputPlayback *data, gchar *uri);
void mseek(InputPlayback *data, gulong time);
void seek(InputPlayback *data, gint time);
void stop(InputPlayback *data);
void cue_pause(InputPlayback *data, short);
Tuple *get_song_tuple(const gchar *uri);
void cue_init(void);
gint get_time(InputPlayback *playback);
void cue_cleanup(void);
gpointer watchdog_func(gpointer data);
Tuple *probe_for_tuple(const gchar *songFilename, VFSFile *fd);

#endif
