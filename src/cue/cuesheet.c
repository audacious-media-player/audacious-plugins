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

#include "config.h"

/* #define DEBUG 1 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/playlist.h>
#include <audacious/util.h>
#include <audacious/strings.h>
#include <audacious/main.h>

#define MAX_CUE_LINE_LENGTH 1000
#define MAX_CUE_TRACKS 1000

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
static Tuple *get_tuple(gchar *uri);
static Tuple *get_aud_tuple_uri(gchar *uri);
static void cue_init(void);
static void cue_cleanup(void);
static gpointer watchdog_func(gpointer data);

static GThread *watchdog_thread = NULL;
static GThread *play_thread = NULL;
static GThread *real_play_thread = NULL;

static GMutex *cue_mutex;
static GCond *cue_cond;
static enum {
    STOP,
    RUN,
    EXIT,
} watchdog_state;

static GMutex *cue_block_mutex;
static GCond *cue_block_cond;

static InputPlayback *caller_ip = NULL;

static gchar *cue_file = NULL;
static gchar *cue_title = NULL;
static gchar *cue_performer = NULL;
static gchar *cue_genre = NULL;
static gchar *cue_year = NULL;
static gchar *cue_track = NULL;

static gint last_cue_track = 0;
static gint cur_cue_track = 0;
static gint target_time = 0;
static GMutex *cue_target_time_mutex;

static struct {
	gchar *title;
	gchar *performer;
	gint index;
} cue_tracks[MAX_CUE_TRACKS];

static gint finetune_seek = 0;

static InputPlayback *real_ip = NULL;

InputPlugin cue_ip =
{
	.description = "Cuesheet Plugin",	/* description */
	.init = cue_init,	       	/* init */
	.is_our_file = is_our_file,
	.play_file = play,
	.stop = stop,
	.pause = cue_pause,
	.seek = seek,
	.get_time = get_time,
	.cleanup = cue_cleanup,		/* cleanup */
	.get_song_tuple = get_tuple,
};

InputPlugin *cue_iplist[] = { &cue_ip, NULL };

DECLARE_PLUGIN(cue, NULL, NULL, cue_iplist, NULL, NULL, NULL, NULL, NULL);

static void cue_init(void)
{
    cue_mutex = g_mutex_new();
    cue_cond = g_cond_new();
    cue_block_mutex = g_mutex_new();
    cue_block_cond = g_cond_new();
    cue_target_time_mutex = g_mutex_new();

    /* create watchdog thread */
    g_mutex_lock(cue_mutex);
    watchdog_state = STOP;
    g_mutex_unlock(cue_mutex);
    watchdog_thread = g_thread_create(watchdog_func, NULL, TRUE, NULL);
#ifdef DEBUG
    g_print("watchdog_thread = %p\n", watchdog_thread);
#endif
    uri_set_plugin("cue://", &cue_ip);
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
    g_cond_free(cue_block_cond);
    g_mutex_free(cue_block_mutex);
    g_mutex_free(cue_target_time_mutex);
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
		cache_cue_file(filename); //filename should be uri.

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
    gchar *uri = g_strdup(data->filename);

#ifdef DEBUG
    g_print("play: playback = %p\n", data);
    g_print("play: uri = %s\n", uri);
#endif

    caller_ip = data;
	/* this isn't a cue:// uri? */
	if (strncasecmp("cue://", uri, 6))
	{
		gchar *tmp = g_strdup_printf("cue://%s?0", uri);
		g_free(uri);
		uri = tmp;
	}
	play_thread = g_thread_self();
	data->set_pb_ready(data); // it should be done in real input plugin? --yaz
	play_cue_uri(data, uri);
	g_free(uri); uri = NULL;
}

static Tuple *get_tuple(gchar *uri)
{
	Tuple *ret;

	/* this isn't a cue:// uri? */
	if (strncasecmp("cue://", uri, 6))
	{
		gchar *tmp = g_strdup_printf("cue://%s?0", uri);
		ret = get_aud_tuple_uri(tmp);
		g_free(tmp);
		return ret;
	}

	return get_aud_tuple_uri(uri);
}

static void _aud_tuple_copy_field(Tuple *tuple, Tuple *tuple2, const gint nfield, const gchar *field)
{
    const gchar *str = aud_tuple_get_string(tuple, nfield, field);
    aud_tuple_disassociate(tuple2, nfield, field);
    aud_tuple_associate_string(tuple2, nfield, field, str);
}

static Tuple *get_aud_tuple_uri(gchar *uri)
{
    gchar *path2 = g_strdup(uri + 6);
    gchar *_path = strchr(path2, '?');
    gint track = 0;

	ProbeResult *pr;
	InputPlugin *dec;

	Tuple *phys_tuple, *out;

        if (_path != NULL && *_path == '?')
        {
                *_path = '\0';
                _path++;
                track = atoi(_path);
        }	

	cache_cue_file(path2); //path2 should be uri.

	if (cue_file == NULL)
		return NULL;
#ifdef DEBUG    
	g_print("cue_file = %s\n", cue_file);
#endif
	pr = input_check_file(cue_file, FALSE);
	if (pr == NULL)
		return NULL;
	dec = pr->ip;
	if (dec == NULL)
		return NULL;

	if (dec->get_song_tuple)
		phys_tuple = dec->get_song_tuple(cue_file);
	else
		phys_tuple = input_get_song_tuple(cue_file);

    if(!phys_tuple)
        return NULL;

    out = aud_tuple_new();

    _aud_tuple_copy_field(phys_tuple, out, FIELD_FILE_PATH, NULL);
    _aud_tuple_copy_field(phys_tuple, out, FIELD_FILE_NAME, NULL);
    _aud_tuple_copy_field(phys_tuple, out, FIELD_FILE_EXT, NULL);
    _aud_tuple_copy_field(phys_tuple, out, FIELD_CODEC, NULL);
    _aud_tuple_copy_field(phys_tuple, out, FIELD_QUALITY, NULL);
    _aud_tuple_copy_field(phys_tuple, out, FIELD_COPYRIGHT, NULL);
    _aud_tuple_copy_field(phys_tuple, out, FIELD_COMMENT, NULL);

    aud_tuple_associate_int(out, FIELD_LENGTH, NULL, aud_tuple_get_int(phys_tuple, FIELD_LENGTH, NULL));

    aud_tuple_free(phys_tuple);

    aud_tuple_associate_string(out, FIELD_TITLE, NULL, cue_tracks[track].title);
    aud_tuple_associate_string(out, FIELD_ARTIST, NULL, cue_tracks[track].performer ?
				  cue_tracks[track].performer : cue_performer);
    aud_tuple_associate_string(out, FIELD_ALBUM, NULL, cue_title);
    aud_tuple_associate_string(out, FIELD_GENRE, NULL, cue_genre);
    if(cue_year)
        aud_tuple_associate_int(out, FIELD_YEAR, NULL, atoi(cue_year));
    aud_tuple_associate_int(out, FIELD_TRACK_NUMBER, NULL, track + 1);

    return out;
}

static void seek(InputPlayback * data, gint time)
{
    g_mutex_lock(cue_target_time_mutex);
    target_time = time * 1000;
    g_mutex_unlock(cue_target_time_mutex);

#ifdef DEBUG
    g_print("seek: playback = %p\n", data);
    g_print("cue: seek: target_time = %d\n", target_time);
#endif

	if (real_ip != NULL) {
		real_ip->plugin->seek(real_ip, time);
    }
}

static void stop(InputPlayback * data)
{
#ifdef DEBUG
    g_print("f: stop: playback = %p\n", data);
#endif

    if(play_thread) {
        if(real_play_thread) {
            g_cond_signal(cue_block_cond); /* kick play_cue_uri */

            if (real_ip != NULL)
                real_ip->plugin->stop(real_ip);
#ifdef DEBUG
            g_print("i: stop(real_ip) finished\n");
#endif
            real_play_thread = NULL;

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
        } /* real_play_thread */

        g_thread_join(play_thread);
        play_thread = NULL;

    } /*play_thread*/


#ifdef DEBUG
    g_print("e: stop\n");
#endif
}

/* not publicly available functions. */
extern void playback_stop(void);
extern void mainwin_clear_song_info(void);

static gboolean do_stop(gpointer data)
{
#ifdef DEBUG
    g_print("f: do_stop\n");
#endif

    ip_data.stop = TRUE;
    playback_stop();
    ip_data.stop = FALSE;

    mainwin_clear_song_info();

#ifdef DEBUG
    g_print("e: do_stop\n");
#endif
    return FALSE; //one-shot
}

static gboolean do_setpos(gpointer data)
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

    if (!playlist)
        return FALSE;

    /* being done from the main loop thread, does not require locks */
    playlist_set_position(playlist, (guint)pos);

    return FALSE; //one-shot
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
    gchar *path2 = g_strdup(uri + 6); // "cue://" is stripped.
    gchar *_path = strchr(path2, '?');
	gint track = 0;
	ProbeResult *pr;
	InputPlugin *real_ip_plugin;
    Tuple *tuple = NULL;

#ifdef DEBUG
    g_print("f: play_cue_uri\n");
    g_print("play_cue_uri: playback = %p\n", data);
    g_print("play_cue_uri: path2 = %s\n", path2);
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
	cache_cue_file(path2); //path2 should be uri.

    if (cue_file == NULL || !vfs_file_test(cue_file, G_FILE_TEST_EXISTS))
        return;

	pr = input_check_file(cue_file, FALSE);
	if (pr == NULL)
		return;

	real_ip_plugin = pr->ip;

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

		/* we have to copy set_pb_ready things too. */
		real_ip->pb_ready_mutex = data->pb_ready_mutex;
		real_ip->pb_ready_cond = data->pb_ready_cond;
		real_ip->pb_ready_val = data->pb_ready_val;
		real_ip->set_pb_ready = data->set_pb_ready;

		real_play_thread = g_thread_create((GThreadFunc)(real_ip->plugin->play_file), (gpointer)real_ip, TRUE, NULL);
		g_usleep(50000); // wait for 50msec while real input plugin is initializing.

		if(real_ip->plugin->mseek) {
#ifdef DEBUG
            g_print("mseek\n");
#endif
			real_ip->plugin->mseek(real_ip, finetune_seek ? finetune_seek : cue_tracks[track].index);
		}
		else
			real_ip->plugin->seek(real_ip, finetune_seek ? finetune_seek / 1000 : cue_tracks[track].index / 1000 + 1);

        g_mutex_lock(cue_target_time_mutex);
        target_time = finetune_seek ? finetune_seek : cue_tracks[track].index;
        g_mutex_unlock(cue_target_time_mutex);
#ifdef DEBUG
        g_print("cue: play_cue_uri: target_time = %d\n", target_time);
#endif

        tuple = real_ip->plugin->get_song_tuple(cue_file);
        if(tuple) {
            cue_tracks[last_cue_track].index = aud_tuple_get_int(tuple, FIELD_LENGTH, NULL);
            aud_tuple_free(tuple); tuple = NULL;
        }

        /* kick watchdog thread */
        g_mutex_lock(cue_mutex);
        watchdog_state = RUN;
        g_mutex_unlock(cue_mutex);
        g_cond_signal(cue_cond);
#ifdef DEBUG
        g_print("watchdog activated\n");
#endif
        finetune_seek = 0;
        if(real_play_thread) {
            g_mutex_lock(cue_block_mutex);
            g_cond_wait(cue_block_cond, cue_block_mutex); // block until stop() is called.
            g_mutex_unlock(cue_block_mutex);            
        }
	}

#ifdef DEBUG
    g_print("e: play_cue_uri\n");
#endif
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
        g_time_val_add(&sleep_time, 10000); // interval is 10msec.

        g_mutex_lock(cue_mutex);
        switch(watchdog_state) {
        case EXIT:
#ifdef DEBUG
            g_print("e: watchdog exit\n");
#endif
            g_mutex_unlock(cue_mutex); // stop() will lock cue_mutex.
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
#if 0
#ifdef DEBUG
        g_print("time = %d target_time = %d\n", time, target_time);
#endif
#endif
        if(time == 0 || time <= target_time)
            continue;

        // prev track
        if (time < cue_tracks[cur_cue_track].index)
        {
            static gint incr = 0;
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
#ifdef DEBUG
                g_print("cue: prev_track: time = %d cue_tracks[cur_cue_track].index = %d\n",
                       time, cue_tracks[cur_cue_track].index);
                g_print("cue: prev_track: finetune_seek = %d\n", finetune_seek);
#endif
            }

            g_mutex_lock(cue_target_time_mutex);
            target_time = finetune_seek ? finetune_seek : cue_tracks[cur_cue_track].index;
            g_mutex_unlock(cue_target_time_mutex);
#ifdef DEBUG
            g_print("cue: prev_track: target_time = %d\n", target_time);
#endif
            g_idle_add_full(G_PRIORITY_HIGH , do_setpos, &incr, NULL);
            continue;
        }

        // next track
        if (cur_cue_track + 1 < last_cue_track && time > cue_tracks[cur_cue_track + 1].index)
        {
            static gint incr = 0;
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
                if (time >= cue_tracks[cur_cue_track].index)
                    finetune_seek = time;
#ifdef DEBUG
                g_print("cue: next_track: time = %d cue_tracks[cur_cue_track].index = %d\n",
                       time, cue_tracks[cur_cue_track].index);
                g_print("cue: next_track: finetune_seek = %d\n", finetune_seek);
#endif
            }

            g_mutex_lock(cue_target_time_mutex);
            target_time = finetune_seek ? finetune_seek : cue_tracks[cur_cue_track].index;
            g_mutex_unlock(cue_target_time_mutex);
#ifdef DEBUG
            g_print("cue: next_track: target_time = %d\n", target_time);
#endif
            if(cfg.stopaftersong) {
                g_idle_add_full(G_PRIORITY_HIGH, do_stop, (void *)real_ip, NULL);
                continue;
            }
            else {
                g_idle_add_full(G_PRIORITY_HIGH , do_setpos, &incr, NULL);
                continue;
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
                        static gint incr = 0;
                        incr = -pos;
                        g_idle_add_full(G_PRIORITY_HIGH , do_setpos, &incr, NULL);
                        continue;
                    }
                    else {
                        g_idle_add_full(G_PRIORITY_HIGH, do_stop, (void *)real_ip, NULL);
                        continue;
                    }
                }
                else {
                    if(cfg.stopaftersong) {
                        g_idle_add_full(G_PRIORITY_HIGH, do_stop, (void *)real_ip, NULL);
                        continue;
                    }
#ifdef DEBUG
                    g_print("i: watchdog end of cue, advance in playlist\n\n");
#endif
                    static gint incr = 1;
                    g_idle_add_full(G_PRIORITY_HIGH , do_setpos, &incr, NULL);
                    continue;
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
			for (p = q; line[p] && !isspace((int) line[p]); p++);
			if (!line[p])
				continue;
			line[p] = '\0';
			for (p++; line[p] && isspace((int) line[p]); p++);
			if(strcasecmp(line+q, "GENRE") == 0) {
				fix_cue_argument(line+p);
				if (last_cue_track == 0)
					cue_genre = str_to_utf8(line + p);
			}
			if(strcasecmp(line+q, "DATE") == 0) {
				gchar *tmp;
				fix_cue_argument(line+p);
				if (last_cue_track == 0) {
					tmp = g_strdup(line + p);
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
			cue_file = g_strdup_printf("%s/%s", tmp, line+q); //XXX need to check encoding?
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
            gint min, sec, frac;
			for (p = q; line[p] && !isspace((int) line[p]); p++);
			if (!line[p])
				continue;
			for (p++; line[p] && isspace((int) line[p]); p++);
			for (q = p; line[q] && !isspace((int) line[q]); q++);

            if(sscanf(line+p, "%d:%d:%d", &min, &sec, &frac) == 3) {
//                printf("%3d:%02d:%02d\n", min, sec, frac);
                cue_tracks[last_cue_track-1].index = min * 60000 + sec * 1000 + frac * 10;
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
