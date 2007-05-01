/* Audacious: An advanced media player.
 * cuesheet.c: Support cuesheets as a media container.
 *
 * Copyright (C) 2006 William Pitcock <nenolod -at- nenolod.net>.
 *                    Jonathan Schleifer <js@h3c.de> (only small fixes)
 *
 * Copyright (C) 2007 Yoshiki Yazawa <yaz@cc.rim.or.jp> (millisecond
 * seek and multithreading)
 *
 * This file was hacked out of of xmms-cueinfo,
 * Copyright (C) 2003  Oskar Liljeblad
 *
 * This software is copyrighted work licensed under the terms of the
 * GNU General Public License. Please consult the file "COPYING" for
 * details.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* #define DEBUG 1 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/playlist.h>
#include <audacious/vfs.h>
#include <audacious/util.h>
#include <audacious/strings.h>
#include <audacious/main.h>
#include <audacious/strings.h>

#define MAX_CUE_LINE_LENGTH 1000
#define MAX_CUE_TRACKS 1000
#define TRANSITION_GUARD_TIME 500000

static void cache_cue_file(gchar *f);
static void free_cue_info(void);
static void fix_cue_argument(char *line);
static gboolean is_our_file(gchar *filespec);
static void play(InputPlayback *data);
static void play_cue_uri(InputPlayback *data, gchar *uri);
static gint get_time(InputPlayback *data);
static void seek(InputPlayback *data, gint time);
static void stop(InputPlayback *data);
static void cue_pause(InputPlayback *data, short);
static TitleInput *get_tuple(gchar *uri);
static TitleInput *get_tuple_uri(gchar *uri);
static void get_song_info(gchar *uri, gchar **title, gint *length);
static void cue_init(void);
static void cue_cleanup(void);
static gpointer watchdog_func(gpointer data);

GThread *watchdog_thread;
static GMutex *cue_mutex;
static GCond *cue_cond;
static enum {
    STOP,
    RUN,
    EXIT,
} watchdog_state;

static InputPlayback *caller_ip = NULL;
GThread *exec_thread;

static gchar *cue_file = NULL;
static gchar *cue_title = NULL;
static gchar *cue_performer = NULL;
static gchar *cue_genre = NULL;
static gchar *cue_year = NULL;
static gchar *cue_track = NULL;

static gint last_cue_track = 0;
static gint cur_cue_track = 0;

static struct {
	gchar *title;
	gchar *performer;
	gint index;
} cue_tracks[MAX_CUE_TRACKS];

static gint finetune_seek = 0;

static InputPlayback *real_ip = NULL;

InputPlugin cue_ip =
{
	NULL,			/* handle */
	NULL,			/* filename */
	NULL,			/* description */
	cue_init,	       	/* init */
	NULL,	       	/* about */
	NULL,	  	   	/* configure */
	is_our_file,
	NULL,		/* audio cd */
	play,
	stop,
	cue_pause,
	seek,
	NULL,		/* set eq */
	get_time,
	NULL,
	NULL,
	cue_cleanup,		/* cleanup */
	NULL,
	NULL,
	NULL,
	NULL,
	get_song_info,	/* XXX get_song_info iface */
	NULL,
	NULL,
	get_tuple,
	NULL
};

static void cue_init(void)
{
    cue_mutex = g_mutex_new();
    cue_cond = g_cond_new();

    /* create watchdog thread */
    g_mutex_lock(cue_mutex);
    watchdog_state = STOP;
    g_mutex_unlock(cue_mutex);
    watchdog_thread = g_thread_create(watchdog_func, NULL, TRUE, NULL);
#ifdef DEBUG
    g_print("watchdog_thread = %p\n", watchdog_thread);
#endif
}

static void cue_cleanup(void)
{
    g_mutex_lock(cue_mutex);
    watchdog_state = EXIT;
    g_mutex_unlock(cue_mutex);
    g_cond_broadcast(cue_cond);

    g_thread_join(watchdog_thread);

    g_cond_free(cue_cond);
    g_mutex_free(cue_mutex);
}

static int is_our_file(gchar *filename)
{
	gchar *ext;

	/* is it a cue:// URI? */
	if (!strncasecmp(filename, "cue://", 6))
		return TRUE;

	ext = strrchr(filename, '.');

	if(!ext)
		return FALSE;
	
	if (!strncasecmp(ext, ".cue", 4))
	{
		gint i;

		/* add the files, build cue urls, etc. */
		cache_cue_file(filename);

		for (i = 0; i < last_cue_track; i++)
		{
			gchar _buf[65535];

			g_snprintf(_buf, 65535, "cue://%s?%d", filename, i);
			playlist_add_url(playlist_get_active(), _buf);
		}

		free_cue_info();

		return -1;
	}

	return FALSE;
}

static gint get_time(InputPlayback *playback)
{
	return playback->output->output_time();
}

static void play(InputPlayback *data)
{
    gchar *uri = data->filename;

#ifdef DEBUG
    g_print("play: playback = %p\n", data);
#endif

    caller_ip = data;
	/* this isn't a cue:// uri? */
	if (strncasecmp("cue://", uri, 6))
	{
		gchar *tmp = g_strdup_printf("cue://%s?0", uri);
		play_cue_uri(data, tmp);
		g_free(tmp);
		return;
	}

	play_cue_uri(data, uri);
}

static TitleInput *get_tuple(gchar *uri)
{
	TitleInput *ret;

	/* this isn't a cue:// uri? */
	if (strncasecmp("cue://", uri, 6))
	{
		gchar *tmp = g_strdup_printf("cue://%s?0", uri);
		ret = get_tuple_uri(tmp);
		g_free(tmp);
		return ret;
	}

	return get_tuple_uri(uri);
}

static TitleInput *get_tuple_uri(gchar *uri)
{
    gchar *path2 = g_strdup(uri + 6);
    gchar *_path = strchr(path2, '?');
    gint track = 0;

	InputPlugin *dec;
	TitleInput *phys_tuple, *out;

        if (_path != NULL && *_path == '?')
        {
                *_path = '\0';
                _path++;
                track = atoi(_path);
        }	

	cache_cue_file(path2);

	if (cue_file == NULL)
		return NULL;

	dec = input_check_file(cue_file, FALSE);

	if (dec == NULL)
		return NULL;

	if (dec->get_song_tuple)
		phys_tuple = dec->get_song_tuple(cue_file);
	else
		phys_tuple = input_get_song_tuple(cue_file);

	out = bmp_title_input_new();

	out->file_path = g_strdup(phys_tuple->file_path);	
	out->file_name = g_strdup(phys_tuple->file_name);
	out->file_ext = g_strdup(phys_tuple->file_ext);
	out->length = phys_tuple->length;

	bmp_title_input_free(phys_tuple);

	out->track_name = g_strdup(cue_tracks[track].title);
	out->performer = g_strdup(cue_tracks[track].performer ?
				  cue_tracks[track].performer : cue_performer);
	out->album_name = g_strdup(cue_title);
	out->genre = g_strdup(cue_genre);
	out->year = cue_year ? atoi(cue_year) : 0;
	out->track_number = track + 1;
	return out;
}

static void get_song_info(gchar *uri, gchar **title, gint *length)
{
	TitleInput *tuple;

	/* this isn't a cue:// uri? */
	if (strncasecmp("cue://", uri, 6))
	{
		gchar *tmp = g_strdup_printf("cue://%s?0", uri);
		tuple = get_tuple_uri(tmp);
		g_free(tmp);
	}
	else
		tuple = get_tuple_uri(uri);

	g_return_if_fail(tuple != NULL);

	*title = xmms_get_titlestring(xmms_get_gentitle_format(), tuple);
	*length = tuple->length;

	bmp_title_input_free(tuple);
}

static void seek(InputPlayback * data, gint time)
{
#ifdef DEBUG
    g_print("seek: playback = %p\n", data);
#endif

	if (real_ip != NULL)
		real_ip->plugin->seek(real_ip, time);
}

static void stop(InputPlayback * data)
{
#ifdef DEBUG
    g_print("f: stop: playback = %p\n", data);
#endif
	if (real_ip != NULL)
		real_ip->plugin->stop(real_ip);
#ifdef DEBUG
    g_print("i: stop(real_ip) finished\n");
#endif
    if (data != NULL)
        data->playing = 0;
    if (caller_ip != NULL)
        caller_ip->playing = 0;

    g_mutex_lock(cue_mutex);
    watchdog_state = STOP;
    g_mutex_unlock(cue_mutex);
    g_cond_signal(cue_cond);

	free_cue_info();

	if (real_ip != NULL) {
		real_ip->plugin->set_info = cue_ip.set_info;
		real_ip->plugin->output = NULL;
		g_free(real_ip);
		real_ip = NULL;
	}
#ifdef DEBUG
    g_print("e: stop\n");
#endif
}

// not publicly available functions.
extern void playback_stop(void);
extern void mainwin_clear_song_info(void);

static gpointer do_stop(gpointer data)
{
//    InputPlayback *playback = (InputPlayback *)data;
    Playlist *playlist = playlist_get_active();
#ifdef DEBUG
    g_print("f: do_stop\n");
#endif
    ip_data.stop = TRUE;
    playback_stop();
    ip_data.stop = FALSE;

    PLAYLIST_LOCK(playlist->mutex);
    gdk_threads_enter();
    mainwin_clear_song_info();
    gdk_threads_leave();
    PLAYLIST_UNLOCK(playlist->mutex);

#ifdef DEBUG
    g_print("e: do_stop\n");
#endif
    g_thread_exit(NULL);
    return NULL; //dummy
}

static gpointer do_setpos(gpointer data)
{
    Playlist *playlist = playlist_get_active();
    gint pos = playlist_get_position_nolock(playlist);
    gint incr = *(gint *)data;

    pos = pos + incr;
    if(pos < 0)
        pos = 0;

#ifdef DEBUG
    g_print("do_setpos: pos = %d\n\n", pos);
#endif
    playlist_set_position(playlist, (guint)pos);
    g_thread_exit(NULL);
    return NULL; //dummy
}

static void cue_pause(InputPlayback * data, short p)
{
	if (real_ip != NULL)
		real_ip->plugin->pause(real_ip, p);
}

static void set_info_override(gchar * unused, gint length, gint rate, gint freq, gint nch)
{
	gchar *title;
	Playlist *playlist = playlist_get_active();

	g_return_if_fail(playlist != NULL);

	/* annoying. */
	if (playlist->position->tuple == NULL)
	{
		gint pos = playlist_get_position(playlist);
		playlist_get_tuple(playlist, pos);
	}

	title = g_strdup(playlist->position->title);

	cue_ip.set_info(title, length, rate, freq, nch);
}

static void play_cue_uri(InputPlayback * data, gchar *uri)
{
    gchar *path2 = g_strdup(uri + 6);
    gchar *_path = strchr(path2, '?');
	gint file_length = 0;
	gint track = 0;
	gchar *dummy = NULL;
	InputPlugin *real_ip_plugin;

#ifdef DEBUG
    g_print("f: play_cue_uri\n");
    g_print("play_cue_uri: playback = %p\n", data);
#endif

    /* stop watchdog thread */
    g_mutex_lock(cue_mutex);
    watchdog_state = STOP;
    g_mutex_unlock(cue_mutex);
    g_cond_signal(cue_cond);

    if (_path != NULL && *_path == '?')
    {
        *_path = '\0';
        _path++;
        track = atoi(_path);
    }	
	cur_cue_track = track;
	cache_cue_file(path2);

    if (cue_file == NULL)
        return;

	real_ip_plugin = input_check_file(cue_file, FALSE);

	if (real_ip_plugin != NULL)
	{
		if (real_ip)
			g_free(real_ip);
		real_ip = g_new0(InputPlayback, 1);
		real_ip->plugin = real_ip_plugin;
		real_ip->plugin->set_info = set_info_override;
		real_ip->plugin->output = cue_ip.output;
		real_ip->filename = cue_file;

		/* need to pass playback->output to real_ip */
		real_ip->output = data->output;
		real_ip->data = data->data;

		real_ip->plugin->play_file(real_ip);

		if(real_ip->plugin->mseek) { // seek by millisecond
#ifdef DEBUG
			g_print("mseek\n");
#endif
			real_ip->plugin->mseek(real_ip, finetune_seek ? finetune_seek : cue_tracks[track].index);
		}
		else
			real_ip->plugin->seek(real_ip, finetune_seek ? finetune_seek / 1000 : cue_tracks[track].index / 1000 + 1);

		// in some plugins, NULL as 2nd arg causes crash.
		real_ip->plugin->get_song_info(cue_file, &dummy, &file_length);
		g_free(dummy);
		cue_tracks[last_cue_track].index = file_length;

        /* kick watchdog thread */
        g_usleep(TRANSITION_GUARD_TIME);
        g_mutex_lock(cue_mutex);
        watchdog_state = RUN;
        g_mutex_unlock(cue_mutex);
        g_cond_signal(cue_cond);
#ifdef DEBUG
        g_print("watchdog activated\n");
#endif
	}

	finetune_seek = 0;

#ifdef DEBUG
    g_print("e: play_cue_uri\n");
#endif
}

InputPlugin *get_iplugin_info(void)
{
	cue_ip.description = g_strdup_printf("Cuesheet Container Plugin");
	return &cue_ip;
}

/******************************************************* watchdog */

/*
 * This is fairly hard to explain.
 *
 * Basically we loop until we have reached the correct track.
 * Then we set a finetune adjustment to make sure we stay in the
 * right place.
 *
 * I used to recurse here (it was prettier), but that didn't work
 * as well as I was hoping.
 *
 * Anyhow, yeah. The logic here isn't great, but it works, so I'm
 * cool with it.
 *
 *     - nenolod
 */
static gpointer watchdog_func(gpointer data)
{
    gint time = 0;
    Playlist *playlist = NULL;
    GTimeVal sleep_time;

#ifdef DEBUG
    g_print("f: watchdog\n");
#endif

    while(1) {
#if 0
#if DEBUG
        g_print("time = %d cur = %d cidx = %d nidx = %d last = %d\n",
                time, cur_cue_track,
                cue_tracks[cur_cue_track].index,
                cue_tracks[cur_cue_track+1].index, last_cue_track);
#endif
#endif
        g_get_current_time(&sleep_time);
        g_time_val_add(&sleep_time, 10000); // 10msec

        g_mutex_lock(cue_mutex);
        switch(watchdog_state) {
        case EXIT:
#ifdef DEBUG
            g_print("e: watchdog exit\n");
#endif
            g_mutex_unlock(cue_mutex); // stop() locks cue_mutex.
            stop(real_ip); // need not to care about real_ip != NULL here.
            g_thread_exit(NULL);
            break;
        case RUN:
            if(!playlist)
                playlist = playlist_get_active();
            g_cond_timed_wait(cue_cond, cue_mutex, &sleep_time);
            break;
        case STOP:
#ifdef DEBUG
            g_print("watchdog deactivated\n");
#endif
            g_cond_wait(cue_cond, cue_mutex);
            playlist = playlist_get_active();
            break;
        }
        g_mutex_unlock(cue_mutex);

        if(watchdog_state != RUN)
            continue;

        time = get_output_time();
        if(time == 0)
            continue;

        // prev track
        if (time < cue_tracks[cur_cue_track].index)
        {
            gint incr;
            gint oldpos = cur_cue_track;
#ifdef DEBUG
            g_print("i: watchdog prev\n");
            g_print("time = %d cur = %d cidx = %d nidx = %d\n", time, cur_cue_track,
                    cue_tracks[cur_cue_track].index,
                    cue_tracks[cur_cue_track+1].index);
#endif
            while(time < cue_tracks[cur_cue_track].index) {
                cur_cue_track--;
                incr = cur_cue_track - oldpos; // relative position
                if (time >= cue_tracks[cur_cue_track].index)
                    finetune_seek = time;
            }

            exec_thread = g_thread_create(do_setpos, &incr, FALSE, NULL);
            g_usleep(TRANSITION_GUARD_TIME);
        }

        // next track
        if (cur_cue_track + 1 < last_cue_track && time > cue_tracks[cur_cue_track + 1].index)
        {
            gint incr;
            gint oldpos = cur_cue_track;
#ifdef DEBUG
            g_print("i: watchdog next\n");
            g_print("time = %d cur = %d cidx = %d nidx = %d last = %d lidx = %d\n", time, cur_cue_track,
                    cue_tracks[cur_cue_track].index,
                    cue_tracks[cur_cue_track+1].index,
                    last_cue_track, cue_tracks[last_cue_track].index);
#endif
            while(time > cue_tracks[cur_cue_track + 1].index) {
                cur_cue_track++;
                incr = cur_cue_track - oldpos; // relative position
                if (time <= cue_tracks[cur_cue_track].index)
                    finetune_seek = time;
            }

            if(cfg.stopaftersong) {
                exec_thread = g_thread_create(do_stop, (void *)real_ip, FALSE, NULL);
            }
            else {
                exec_thread = g_thread_create(do_setpos, &incr, FALSE, NULL);
                g_usleep(TRANSITION_GUARD_TIME);
            }
        }

        // last track
        if (cur_cue_track + 1 == last_cue_track &&
            (cue_tracks[last_cue_track].index - time < 500 ||
             time > cue_tracks[last_cue_track].index) ){ // may not happen. for safety.
            if(!real_ip->output->buffer_playing()) {
                gint pos = playlist_get_position(playlist);
                if (pos + 1 == playlist_get_length(playlist)) {
#ifdef DEBUG
                    g_print("i: watchdog eof reached\n\n");
#endif
                
                    if(cfg.repeat) {
                        gint incr = -pos;
                        exec_thread = g_thread_create(do_setpos, &incr, FALSE, NULL);
                        g_usleep(TRANSITION_GUARD_TIME);
                    }
                    else {
                        exec_thread = g_thread_create(do_stop, (void *)real_ip, FALSE, NULL);
                        g_usleep(TRANSITION_GUARD_TIME);
                    }
                }
                else {
                    if(cfg.stopaftersong) {
                        exec_thread = g_thread_create(do_stop, (void *)real_ip, FALSE, NULL);
                    }
#ifdef DEBUG
                    g_print("i: watchdog end of cue, advance in playlist\n\n");
#endif
                    gint incr = 1;
                    exec_thread = g_thread_create(do_setpos, &incr, FALSE, NULL);
                    g_usleep(TRANSITION_GUARD_TIME);
                }
            }
        }
    }
#ifdef DEBUG
    g_print("e: watchdog\n");
#endif
    return NULL; // dummy.
}

/******************************************************** cuefile */

static void free_cue_info(void)
{
	g_free(cue_file);	cue_file = NULL;
	g_free(cue_title);	cue_title = NULL;
	g_free(cue_performer);	cue_performer = NULL;
	g_free(cue_genre);	cue_genre = NULL;
	g_free(cue_year); 	cue_year = NULL;
	g_free(cue_track);	cue_track = NULL;

	for (; last_cue_track > 0; last_cue_track--) {
		g_free(cue_tracks[last_cue_track-1].performer);
		cue_tracks[last_cue_track-1].performer = NULL;
		g_free(cue_tracks[last_cue_track-1].title);
		cue_tracks[last_cue_track-1].title = NULL;
	}
#ifdef DEBUG
	g_print("free_cue_info: last_cue_track = %d\n", last_cue_track);
#endif
	last_cue_track = 0;
}

static void cache_cue_file(char *f)
{
	VFSFile *file = vfs_fopen(f, "rb");
	gchar line[MAX_CUE_LINE_LENGTH+1];

	if(!file)
		return;
	
	while (TRUE) {
		gint p;
		gint q;

		if (vfs_fgets(line, MAX_CUE_LINE_LENGTH+1, file) == NULL) {
			vfs_fclose(file);
			return;
        }

		for (p = 0; line[p] && isspace((int) line[p]); p++);
		if (!line[p])
			continue;
		for (q = p; line[q] && !isspace((int) line[q]); q++);
		if (!line[q])
			continue;
		line[q] = '\0';
		for (q++; line[q] && isspace((int) line[q]); q++);
		if (strcasecmp(line+p, "REM") == 0) {
			gint r;
			for (r = q; line[r] && !isspace((int) line[r]); r++);
			if (!line[r])
				continue;
			line[r] = '\0';
			for (r++; line[r] && isspace((int) line[r]); r++);
			if(strcasecmp(line+q, "GENRE") == 0) {
				fix_cue_argument(line+r);
				if (last_cue_track == 0)
					cue_genre = str_to_utf8(line + r);
			}
			if(strcasecmp(line+q, "DATE") == 0) {
				gchar *tmp;
				fix_cue_argument(line+r);
				if (last_cue_track == 0) {
					tmp = g_strdup(line + r);
					if (tmp) {
						cue_year = strtok(tmp, "/"); // XXX: always in the same format?
					}
				}
			}
		}
		else if (strcasecmp(line+p, "PERFORMER") == 0) {
			fix_cue_argument(line+q);

			if (last_cue_track == 0)
				cue_performer = str_to_utf8(line + q);
			else
				cue_tracks[last_cue_track - 1].performer = str_to_utf8(line + q);
		}
		else if (strcasecmp(line+p, "FILE") == 0) {
			gchar *tmp = g_path_get_dirname(f);
			fix_cue_argument(line+q);
			cue_file = g_strdup_printf("%s/%s", tmp, line+q);	/* XXX: yaz might need to UTF validate this?? -nenolod */ /* as far as I know, all FILEs are in plain ASCII - yaz */
			g_free(tmp);
		}
		else if (strcasecmp(line+p, "TITLE") == 0) {
			fix_cue_argument(line+q);
			if (last_cue_track == 0)
				cue_title = str_to_utf8(line + q);
			else
				cue_tracks[last_cue_track-1].title = str_to_utf8(line + q);
		}
		else if (strcasecmp(line+p, "TRACK") == 0) {
			gint track;

			fix_cue_argument(line+q);
			for (p = q; line[p] && isdigit((int) line[p]); p++);
			line[p] = '\0';
			for (; line[q] && line[q] == '0'; q++);
			if (!line[q])
				continue;
			track = atoi(line+q);
			if (track >= MAX_CUE_TRACKS)
				continue;
			last_cue_track = track;
			cue_tracks[last_cue_track-1].index = 0;
			cue_tracks[last_cue_track-1].performer = NULL;
			cue_tracks[last_cue_track-1].title = NULL;
		}
		else if (strcasecmp(line+p, "INDEX") == 0) {
			for (p = q; line[p] && !isspace((int) line[p]); p++);
			if (!line[p])
				continue;
			for (p++; line[p] && isspace((int) line[p]); p++);
			for (q = p; line[q] && !isspace((int) line[q]); q++);
			if (q-p >= 8 && line[p+2] == ':' && line[p+5] == ':') {
				cue_tracks[last_cue_track-1].index =
						((line[p+0]-'0')*10 + (line[p+1]-'0')) * 60000 +
						((line[p+3]-'0')*10 + (line[p+4]-'0')) * 1000 +
						((line[p+6]-'0')*10 + (line[p+7]-'0')) * 10;
			}
		}
	}

	vfs_fclose(file);
}

static void fix_cue_argument(char *line)
{
	if (line[0] == '"') {
		gchar *l2;
		for (l2 = line+1; *l2 && *l2 != '"'; l2++)
				*(l2-1) = *l2;
			*(l2-1) = *l2;
		for (; *line && *line != '"'; line++) {
			if (*line == '\\' && *(line+1)) {
				for (l2 = line+1; *l2 && *l2 != '"'; l2++)
					*(l2-1) = *l2;
				*(l2-1) = *l2;
			}
		}
		*line = '\0';
	}
	else {
		for (; *line && *line != '\r' && *line != '\n'; line++);
		*line = '\0';
	}
}
