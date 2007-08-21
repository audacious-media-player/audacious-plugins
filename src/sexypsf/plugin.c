/*  sexyPSF - PSF1 player
 *  Copyright (C) 2002-2004 xodnizel
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "audacious/output.h"
#include "audacious/plugin.h"
#include "audacious/main.h"
#include "audacious/tuple.h"
#include "audacious/tuple_formatter.h"
#include "audacious/util.h"
#include "audacious/vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "driver.h"

static volatile int seek = 0;
static volatile gboolean playing = FALSE;
static volatile gboolean paused = FALSE;
static volatile gboolean stop = FALSE;
static volatile gboolean nextsong = FALSE;

extern InputPlugin sexypsf_ip;
static gboolean audio_error = FALSE;

static PSFINFO *PSFInfo = NULL;
static gchar *fnsave = NULL;
static GThread *dethread = NULL;
static InputPlayback *playback = NULL;

static gchar *get_title_psf(gchar *fn);

static int is_our_fd(gchar *filename, VFSFile *file) {
    gchar magic[4];
    vfs_fread(magic, 1, 4, file);

    // only allow PSF1 for now
    if (!memcmp(magic, "PSF\x01", 4))
        return 1;
    return 0;
}

void sexypsf_update(unsigned char *buffer, long count)
{
    const int mask = ~((((16 / 8) * 2)) - 1);

    while (count > 0)
    {
        int t = playback->output->buffer_free() & mask;
        if (t > count)     
            produce_audio(playback->output->written_time(),
                          FMT_S16_NE, 2, count, buffer, NULL);
        else
        {
            if (t)
                produce_audio(playback->output->written_time(),
                              FMT_S16_NE, 2, t, buffer, NULL);
            g_usleep((count-t)*1000*5/441/2);
        }
        count -= t;
        buffer += t;
    }
    if (seek)
    {
        if(sexypsf_seek(seek))
        {
            playback->output->flush(seek);
            seek = 0;
        }
        else  // negative time - must make a C time machine
        {
            sexypsf_stop();
            return;
        }
    }
    if (stop)
        sexypsf_stop();
}

static gpointer sexypsf_playloop(gpointer arg)
{
    while (TRUE)
    {
        sexypsf_execute();

        /* we have reached the end of the song or a command was issued */

        playback->output->buffer_free();
        playback->output->buffer_free();

        if (stop)
            break;

        if (seek)
        {
            playback->output->flush(seek);
            if(!(PSFInfo = sexypsf_load(fnsave)))
                break;
            sexypsf_seek(seek); 
            seek = 0;
            continue;
        }

        // timeout at the end of a file
        sleep(4);
        break;
    }

    playback->output->close_audio();
    if (!(stop)) nextsong = TRUE;
    g_thread_exit(NULL);
    return NULL;
}

static void sexypsf_xmms_play(InputPlayback *data)
{
    if (playing)
        return;

    playback = data;
    nextsong = FALSE;
    paused = FALSE;

    if (!playback->output->open_audio(FMT_S16_NE, 44100, 2))
    {
        audio_error = TRUE;
        return;
    }

    fnsave = malloc(strlen(data->filename)+1);
    strcpy(fnsave, data->filename);
    if(!(PSFInfo=sexypsf_load(data->filename)))
    {
        playback->output->close_audio();
        nextsong = 1;
    }
    else
    {
        stop = seek = 0;

        gchar *name = get_title_psf(data->filename);
        sexypsf_ip.set_info(name, PSFInfo->length, 44100*2*2*8, 44100, 2);
        g_free(name);

        playing = 1;
        dethread = g_thread_self();
        data->set_pb_ready(data);
        sexypsf_playloop(NULL);
    }
}

static void sexypsf_xmms_stop(InputPlayback * playback)
{
    if (!playing) return;

    if (paused)
        playback->output->pause(0);
    paused = FALSE;

    stop = TRUE;
    g_thread_join(dethread);
    playing = FALSE;

    if (fnsave)
    {
        free(fnsave);
        fnsave = NULL;
    } 
    sexypsf_freepsfinfo(PSFInfo);
    PSFInfo = NULL;
}

static void sexypsf_xmms_pause(InputPlayback *playback, short p)
{
    if (!playing) return;
    playback->output->pause(p);
    paused = p;
}

static void sexypsf_xmms_seek(InputPlayback * data, int time)
{
    if (!playing) return;
    seek = time * 1000;
}

static int sexypsf_xmms_gettime(InputPlayback *playback)
{
    if (audio_error)
        return -2;
    if (nextsong)
        return -1;
    if (!playing)
        return 0;

    return playback->output->output_time();
}

static void sexypsf_xmms_getsonginfo(char *fn, char **title, int *length)
{
    PSFINFO *tmp;

    if((tmp = sexypsf_getpsfinfo(fn))) {
        *length = tmp->length;
        *title = get_title_psf(fn);
        sexypsf_freepsfinfo(tmp);
    }
}

static Tuple *get_tuple_psf(gchar *fn) {
    Tuple *tuple = NULL;
    PSFINFO *tmp = sexypsf_getpsfinfo(fn);

    if (tmp->length) {
        tuple = tuple_new_from_filename(fn);
	tuple_associate_int(tuple, "length", tmp->length);
	tuple_associate_string(tuple, "artist", tmp->artist);
	tuple_associate_string(tuple, "album", tmp->game);
	tuple_associate_string(tuple, "game", tmp->game);
        tuple_associate_string(tuple, "title", tmp->title);
        tuple_associate_string(tuple, "genre", tmp->genre);
        tuple_associate_string(tuple, "copyright", tmp->copyright);
        tuple_associate_string(tuple, "quality", "sequenced");
        tuple_associate_string(tuple, "codec", "PlayStation Audio");
        tuple_associate_string(tuple, "console", "PlayStation");
        tuple_associate_string(tuple, "dumper", tmp->psfby);
        tuple_associate_string(tuple, "comment", tmp->comment);

        sexypsf_freepsfinfo(tmp);
    }

    return tuple;
}   

static gchar *get_title_psf(gchar *fn) {
    gchar *title = NULL;
    Tuple *tuple = get_tuple_psf(fn);

    if (tuple != NULL) {
        title = tuple_formatter_make_title_string(tuple, get_gentitle_format());
        tuple_free(tuple);
    }
    else
        title = g_path_get_basename(fn);

    return title;
}

gchar *sexypsf_fmts[] = { "psf", "minipsf", NULL };

InputPlugin sexypsf_ip =
{
    NULL,
    NULL,
    "PSF Audio Plugin",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    sexypsf_xmms_play,
    sexypsf_xmms_stop,
    sexypsf_xmms_pause,
    sexypsf_xmms_seek,
    NULL,
    sexypsf_xmms_gettime,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    sexypsf_xmms_getsonginfo,
    NULL,
    NULL,
    get_tuple_psf,
    NULL,
    NULL,
    is_our_fd,
    sexypsf_fmts,
};

InputPlugin *sexypsf_iplist[] = { &sexypsf_ip, NULL };

DECLARE_PLUGIN(sexypsf, NULL, NULL, sexypsf_iplist, NULL, NULL, NULL, NULL, NULL);
