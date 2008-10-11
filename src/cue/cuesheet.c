/* Audacious: An advanced media player.
 * cuesheet.c: Support cuesheets as a media container.
 *
 * Copyright (C) 2006 William Pitcock <nenolod -at- nenolod.net>.
 *                    Jonathan Schleifer <js-audacious@webkeks.org> (few fixes)
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

#include "cuesheet.h"

#define USE_WATCHDOG 1

static GThread *watchdog_thread = NULL;
static GThread *play_thread = NULL;
static GThread *real_play_thread = NULL;

GMutex *cue_mutex;
GCond *cue_cond;

static GMutex *cue_block_mutex;
static GCond *cue_block_cond;

InputPlayback *caller_ip = NULL;

static gchar *cue_file = NULL;
static gchar *cue_title = NULL;
static gchar *cue_performer = NULL;
static gchar *cue_genre = NULL;
static gchar *cue_year = NULL;
static gchar *cue_track = NULL;

gint last_cue_track = 0;
gint cur_cue_track = 0;
gulong target_time = 0;
GMutex *cue_target_time_mutex;
static gint full_length = 0;

cue_tracks_t cue_tracks[MAX_CUE_TRACKS];

watchdog_state_t watchdog_state;

gint finetune_seek = 0;

InputPlayback *real_ip = NULL;
static gchar *cue_fmts[] = {"cue", NULL};

InputPlugin cue_ip =
{
    .description = "Cuesheet2 Plugin",    /* description */
    .init = cue_init,               /* init */
    .play_file = play,
    .stop = stop,
    .pause = cue_pause,
    .seek = seek,
    .mseek = mseek,
    .get_time = get_time,
    .cleanup = cue_cleanup,        /* cleanup */
    .get_song_tuple = get_song_tuple,
    .vfs_extensions = cue_fmts,
    .is_our_file = is_our_file,
    .probe_for_tuple = probe_for_tuple,
    .have_subtune = TRUE
};

InputPlugin *cue_iplist[] = { &cue_ip, NULL };

DECLARE_PLUGIN(cue, NULL, NULL, cue_iplist, NULL, NULL, NULL, NULL, NULL);

void
cue_init(void)
{
    cue_mutex = g_mutex_new();
    cue_cond = g_cond_new();
    cue_block_mutex = g_mutex_new();
    cue_block_cond = g_cond_new();
    cue_target_time_mutex = g_mutex_new();

#if USE_WATCHDOG
    /* create watchdog thread */
    g_mutex_lock(cue_mutex);
    watchdog_state = STOP;
    g_mutex_unlock(cue_mutex);
    watchdog_thread = g_thread_create(watchdog_func, NULL, TRUE, NULL);
    AUDDBG("watchdog_thread = %p\n", watchdog_thread);
#endif
}

void
cue_cleanup(void)
{
#if USE_WATCHDOG
    g_mutex_lock(cue_mutex);
    watchdog_state = EXIT;
    g_mutex_unlock(cue_mutex);
    g_cond_broadcast(cue_cond);

    g_thread_join(watchdog_thread);
#endif
    g_cond_free(cue_cond);
    g_mutex_free(cue_mutex);
    g_cond_free(cue_block_cond);
    g_mutex_free(cue_block_mutex);
    g_mutex_free(cue_target_time_mutex);
}

Tuple *
probe_for_tuple(gchar *uri, VFSFile *fd)
{
    Tuple *tuple = NULL;

    AUDDBG("uri=%s\n",uri);

    if(!is_our_file(uri))
        return NULL;

	/* Get subtune information */
	tuple = get_song_tuple(uri);
	return tuple;
}


int
is_our_file(gchar *filename)
{
	gchar *ext;

	ext = strrchr(filename, '.');
	if(!ext)
		return FALSE;

	if (!strncasecmp(ext, ".cue", 4))
		return TRUE;

	return FALSE;
}

gint
get_time(InputPlayback *playback)
{
    gint raw_time = playback->output->output_time();
    gint cooked_time;

    /* translate actual time into subtune time */
    cooked_time = raw_time - cue_tracks[cur_cue_track].index;

    /* return raw_time; */
    return cooked_time; /* visualization will be missing. --yaz */
}

void
play(InputPlayback *data)
{
    gchar *uri = g_strdup(data->filename);

    AUDDBG("play: playback = %p\n", data);
    AUDDBG("play: uri = %s\n", uri);

    caller_ip = data;

    play_thread = g_thread_self();
    data->set_pb_ready(data); /* it should be done in real
                                 input plugin? --yaz */
    play_cue_uri(data, uri);
    g_free(uri); uri = NULL;
}

void
_aud_tuple_copy_field(Tuple *tuple, Tuple *tuple2, const gint nfield, const gchar *field)
{
    const gchar *str = aud_tuple_get_string(tuple, nfield, field);
    aud_tuple_disassociate(tuple2, nfield, field);
    aud_tuple_associate_string(tuple2, nfield, field, str);
}


/* this function will be called back for subtune-info in adding to
 * playlist. */
Tuple *
get_song_tuple(gchar *uri) /* *.cue or *.cue?1- */
{
    Tuple *phys_tuple, *out;
    ProbeResult *pr = NULL;
    InputPlugin *dec = NULL;
    gint track = 0;

    /* check subtune */
    gchar *path2 = g_strdup(uri);
    gchar *_path = strchr(path2, '?');

    /* subtune specifed */
    if (_path != NULL && *_path == '?') {
        *_path = '\0';
        _path++;
        track = atoi(_path) - 1; /* subtune number */
    }

    /* parse file of uri and find actual file to play */
    cache_cue_file(path2);

    /* obtain probe result for actual file */
    pr = aud_input_check_file(cue_file, FALSE);
    if (pr == NULL)
        return NULL;
    dec = pr->ip;
    if (dec == NULL)
        return NULL;

    /* get tuple for actual file */
    if (dec->get_song_tuple)
        phys_tuple = dec->get_song_tuple(cue_file);

    if(!phys_tuple)
        return NULL;


    /* build tuple to be returned */
    gchar *realfn = g_filename_from_uri(cue_file, NULL, NULL);
    if(!realfn)
        return NULL;

    gchar *ext = strrchr(realfn, '.');
    ext++;

    /* copy physical tuple */
    out = aud_tuple_new();
    _aud_tuple_copy_field(phys_tuple, out, FIELD_CODEC, NULL);
    _aud_tuple_copy_field(phys_tuple, out, FIELD_QUALITY, NULL);
    _aud_tuple_copy_field(phys_tuple, out, FIELD_COPYRIGHT, NULL);
    _aud_tuple_copy_field(phys_tuple, out, FIELD_COMMENT, NULL);

    full_length = aud_tuple_get_int(phys_tuple, FIELD_LENGTH, NULL);

    aud_tuple_free(phys_tuple);

    /* make path related parts */
    aud_tuple_associate_string(out, FIELD_FILE_PATH, NULL,
                               g_path_get_dirname(realfn));
    aud_tuple_associate_string(out, FIELD_FILE_NAME, NULL,
                               g_path_get_basename(realfn));
    aud_tuple_associate_string(out, FIELD_FILE_EXT, NULL, ext);

    /* set subtune information */
    out->nsubtunes = last_cue_track;
    out->subtunes = NULL; /* ok? */

    /* subtune specified */
    if(_path) {
        aud_tuple_associate_string(out, FIELD_TITLE, NULL,
                                   cue_tracks[track].title);
        aud_tuple_associate_string(out, FIELD_ARTIST, NULL,
                                   cue_tracks[track].performer ?
                                   cue_tracks[track].performer : cue_performer);
        aud_tuple_associate_string(out, FIELD_ALBUM, NULL, cue_title);
        aud_tuple_associate_string(out, FIELD_GENRE, NULL, cue_genre);
        if(cue_year)
            aud_tuple_associate_int(out, FIELD_YEAR, NULL, atoi(cue_year));
        aud_tuple_associate_int(out, FIELD_TRACK_NUMBER, NULL, track+1);
        aud_tuple_associate_int(out, FIELD_LENGTH, NULL,
                                cue_tracks[track].duration);
    }
    return out;
}


void
mseek(InputPlayback *input, gulong time)
{
    AUDDBG("cur_cue_track=%d\n",cur_cue_track);

    g_mutex_lock(cue_target_time_mutex);
    target_time = time + cue_tracks[cur_cue_track].index;
    g_mutex_unlock(cue_target_time_mutex);

    AUDDBG("cue: mseek: target_time = %lu\n", target_time);

    if (real_ip != NULL) {
        if(real_ip->plugin->mseek)
            real_ip->plugin->mseek(real_ip, target_time);
        else
            real_ip->plugin->seek(real_ip, target_time/1000);
    }
}

void
seek(InputPlayback *input, gint time)
{
    gulong millisecond = time * 1000;
    mseek(input, millisecond);
}

void
stop(InputPlayback * data)
{
    AUDDBG("f: stop: playback = %p\n", data);

    if(play_thread) {
        if(real_play_thread) {
            g_cond_signal(cue_block_cond); /* kick play_cue_uri */

            if (real_ip != NULL)
                real_ip->plugin->stop(real_ip);

            AUDDBG("i: stop(real_ip) finished\n");

            real_play_thread = NULL;

            if (data != NULL)
                data->playing = 0;
            if (caller_ip != NULL)
                caller_ip->playing = 0;

#if USE_WATCHDOG
            g_mutex_lock(cue_mutex);
            watchdog_state = STOP;
            g_mutex_unlock(cue_mutex);
            g_cond_signal(cue_cond);
#endif
            free_cue_info();

            if (real_ip != NULL) {
                real_ip->plugin->set_info = cue_ip.set_info;
                g_free(real_ip);
                real_ip = NULL;
            }
        } /* real_play_thread */

        g_thread_join(play_thread);
        play_thread = NULL;

    } /*play_thread*/


    AUDDBG("e: stop\n");
}

void
cue_pause(InputPlayback * data, short p)
{
    if (real_ip != NULL)
        real_ip->plugin->pause(real_ip, p);
}

void
set_info_override(gchar * unused, gint length, gint rate, gint freq, gint nch)
{
    gchar *title;
    Playlist *playlist = aud_playlist_get_active();
    gint cur_len;

    g_return_if_fail(playlist != NULL);

    /* annoying.. */
    if (playlist->position->tuple == NULL)
    {
        gint pos = aud_playlist_get_position(playlist);
        aud_playlist_get_tuple(playlist, pos);
    }

    title = g_strdup(playlist->position->title);
    cur_len = cue_tracks[cur_cue_track].duration;
    cue_ip.set_info(title, cur_len, rate, freq, nch);
}


void
play_cue_uri(InputPlayback * data, gchar *uri)
{
    gchar *path2 = g_strdup(uri); /* file:// */
    gchar *_path = strchr(path2, '?');
    gint track = 0;
    ProbeResult *pr;
    InputPlugin *real_ip_plugin;
    Tuple *tuple = NULL;

    AUDDBG("f: play_cue_uri\n");
    AUDDBG("play_cue_uri: playback = %p\n", data);
    AUDDBG("play_cue_uri: path2 = %s\n", path2);

    /* stop watchdog thread */
#if USE_WATCHDOG
    g_mutex_lock(cue_mutex);
    watchdog_state = STOP;
    g_mutex_unlock(cue_mutex);
    g_cond_signal(cue_cond);
#endif

    if (_path != NULL && *_path == '?')
    {
        *_path = '\0';
        _path++;
        track = atoi(_path) - 1;
        AUDDBG("track=%d\n",track);
    }
    cur_cue_track = track;
    cache_cue_file(path2); /* path2 should be uri. */

    if (cue_file == NULL || !aud_vfs_file_test(cue_file, G_FILE_TEST_EXISTS))
        return;

    pr = aud_input_check_file(cue_file, FALSE); /* find actual input plugin */
    if (pr == NULL)
        return;

    real_ip_plugin = pr->ip;

    if (real_ip_plugin != NULL)
    {
        if (real_ip)
            g_free(real_ip);

        /* duplicate original playback and modify */
        real_ip = (InputPlayback *)g_memdup(data, sizeof(InputPlayback));
        real_ip->plugin = real_ip_plugin;
        real_ip->plugin->set_info = set_info_override;
        real_ip->filename = cue_file;

        data->playing = 1;

        real_play_thread =
            g_thread_create((GThreadFunc)(real_ip->plugin->play_file),
                            (gpointer)real_ip, TRUE, NULL);
        g_usleep(50000); /* wait for 50msec while real input plugin
                            is initializing. */

        if(real_ip->plugin->mseek) {
            AUDDBG("mseek\n");
            real_ip->plugin->mseek(real_ip, finetune_seek ?
                                   finetune_seek : cue_tracks[track].index);
        }
        else
            real_ip->plugin->seek(real_ip, finetune_seek ?
                                  finetune_seek / 1000 :
                                  cue_tracks[track].index / 1000 + 1);

        g_mutex_lock(cue_target_time_mutex);
        target_time = finetune_seek ? finetune_seek : cue_tracks[track].index;
        g_mutex_unlock(cue_target_time_mutex);

        AUDDBG("cue: play_cue_uri: target_time = %lu\n", target_time);

        tuple = real_ip->plugin->get_song_tuple(cue_file);
        if(tuple) {
            cue_tracks[last_cue_track].index =
                aud_tuple_get_int(tuple, FIELD_LENGTH, NULL);
            aud_tuple_free(tuple); tuple = NULL;
        }

        /* kick watchdog thread */
#if USE_WATCHDOG
        g_mutex_lock(cue_mutex);
        watchdog_state = RUN;
        g_mutex_unlock(cue_mutex);
        g_cond_signal(cue_cond);

        AUDDBG("watchdog activated\n");
#endif
        finetune_seek = 0;
        if(real_play_thread) {
            g_mutex_lock(cue_block_mutex);
            /* block until stop() is called. */
            g_cond_wait(cue_block_cond, cue_block_mutex);
            g_mutex_unlock(cue_block_mutex);
        }
    }

    AUDDBG("e: play_cue_uri\n");
}


/******************************************************** cuefile */

void
free_cue_info(void)
{
    g_free(cue_file);    cue_file = NULL;
    g_free(cue_title);    cue_title = NULL;
    g_free(cue_performer);    cue_performer = NULL;
    g_free(cue_genre);    cue_genre = NULL;
    g_free(cue_year);     cue_year = NULL;
    g_free(cue_track);    cue_track = NULL;

    for (; last_cue_track > 0; last_cue_track--) {
        g_free(cue_tracks[last_cue_track-1].performer);
        cue_tracks[last_cue_track-1].performer = NULL;
        g_free(cue_tracks[last_cue_track-1].title);
        cue_tracks[last_cue_track-1].title = NULL;
    }
    AUDDBG("free_cue_info: last_cue_track = %d\n", last_cue_track);
    last_cue_track = 0;
}

void
cache_cue_file(char *f)
{
    VFSFile *file = aud_vfs_fopen(f, "rb");
    gchar line[MAX_CUE_LINE_LENGTH+1];

    if(!file)
        return;

    while (TRUE) {
        gint p, q;

        if (aud_vfs_fgets(line, MAX_CUE_LINE_LENGTH+1, file) == NULL) {
            aud_vfs_fclose(file);
            cue_tracks[last_cue_track-1].duration =
                full_length - cue_tracks[last_cue_track-1].index;
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
                    cue_genre = aud_str_to_utf8(line + p);
            }
            if(strcasecmp(line+q, "DATE") == 0) {
                gchar *tmp;
                fix_cue_argument(line+p);
                if (last_cue_track == 0) {
                    tmp = g_strdup(line + p);
                    if (tmp) {
                        cue_year = strtok(tmp, "/");
                    }
                }
            }
        }
        else if (strcasecmp(line+p, "PERFORMER") == 0) {
            fix_cue_argument(line+q);

            if (last_cue_track == 0)
                cue_performer = aud_str_to_utf8(line + q);
            else
                cue_tracks[last_cue_track - 1].performer = aud_str_to_utf8(line + q);
        }
        else if (strcasecmp(line+p, "FILE") == 0) {
            gchar *tmp = g_path_get_dirname(f);
            fix_cue_argument(line+q);
            cue_file = g_strdup_printf("%s/%s", tmp, line+q);
            g_free(tmp);
        }
        else if (strcasecmp(line+p, "TITLE") == 0) {
            fix_cue_argument(line+q);
            if (last_cue_track == 0)
                cue_title = aud_str_to_utf8(line + q);
            else
                cue_tracks[last_cue_track-1].title = aud_str_to_utf8(line + q);
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
            line[p] = '\0';
            for (p++; line[p] && isspace((int) line[p]); p++);
            if(strcasecmp(line+q, "01") == 0) {
                fix_cue_argument(line+p);
                if(sscanf(line+p, "%d:%d:%d", &min, &sec, &frac) == 3) {
                    cue_tracks[last_cue_track-1].index =
                        min * 60000 + sec * 1000 + frac * 1000 / 75;
                }
            }
            else if(strcasecmp(line+q, "00") == 0) {
                if(sscanf(line+p, "%d:%d:%d", &min, &sec, &frac) == 3) {
                    gint index0 = min * 60000 + sec * 1000 + frac * 10;
                    cue_tracks[last_cue_track-2].duration =
                        index0 - cue_tracks[last_cue_track-2].index;
                }
            }
        }
    }

    aud_vfs_fclose(file);
}

void
fix_cue_argument(char *line)
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
