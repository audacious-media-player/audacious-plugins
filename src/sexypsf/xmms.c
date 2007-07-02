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
#include "audacious/titlestring.h"
#include "audacious/util.h"
#include "audacious/vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "driver.h"

#define CMD_SEEK    0x80000000
#define CMD_STOP    0x40000000

static volatile int seek_time = 0;
static volatile int stop = 0;
static volatile int playing=0;
static volatile int nextsong=0;

extern InputPlugin sexypsf_ip;
static char *fnsave = NULL;

static gchar *get_title_psf(gchar *fn);
static short paused;
static GThread *dethread;
static PSFINFO *PSFInfo = NULL;
static InputPlayback *playback;

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
            produce_audio(playback->output->written_time(), FMT_S16_NE, 2, count, buffer, NULL);
        else
        {
            if (t)
                produce_audio(playback->output->written_time(), FMT_S16_NE, 2, t, buffer, NULL);
            g_usleep((count-t)*1000*5/441/2);
        }
        count -= t;
        buffer += t;
    }
    if (seek_time)
    {
        if(sexypsf_seek(seek_time))
            playback->output->flush(seek_time);
        // negative time - must make a C time machine
        else
        {
            sexypsf_stop();
            return;
        }
        seek_time = 0;
    }
    if (stop)
        sexypsf_stop();
}

static gpointer sexypsf_playloop(gpointer arg)
{
dofunky:

    sexypsf_execute();

    /* we have reached the end of the song */

    playback->output->buffer_free();
    playback->output->buffer_free();

    while (!(stop)) 
    {
        if (seek_time)
        {
            playback->output->flush(seek_time);
            if(!(PSFInfo=sexypsf_load(fnsave)))
                break;
            sexypsf_seek(seek_time); 
            seek_time = 0;
            goto dofunky;
        }
        if (!playback->output->buffer_playing()) break;
        usleep(2000);
    }
    playback->output->close_audio();
    if(!(stop)) nextsong=1;
    g_thread_exit(NULL);
    return NULL;
}

static void sexypsf_xmms_play(InputPlayback *data)
{
    char *fn = data->filename;
    if(playing)
        return;
    playback = data;
    nextsong=0;
    paused = 0;
    if(!playback->output->open_audio(FMT_S16_NE, 44100, 2))
    {
        puts("Error opening audio.");
        return;
    }
    fnsave=malloc(strlen(fn)+1);
    strcpy(fnsave,fn);
    if(!(PSFInfo=sexypsf_load(fn)))
    {
        playback->output->close_audio();
        nextsong=1;
    }
    else
    {
        stop = seek_time = 0;

        gchar *name = get_title_psf(fn);
        sexypsf_ip.set_info(name,PSFInfo->length,44100*2*2*8,44100,2);
        g_free(name);

        playing=1;
        dethread = g_thread_create((GThreadFunc)sexypsf_playloop,NULL,TRUE,NULL);
    }
}

static void sexypsf_xmms_stop(InputPlayback * playback)
{
    if(!playing) return;

    if(paused)
        playback->output->pause(0);
    paused = 0;

    stop = TRUE;
    g_thread_join(dethread);
    playing = 0;

    if(fnsave)
    {
        free(fnsave);
        fnsave=NULL;
    } 
    sexypsf_freepsfinfo(PSFInfo);
    PSFInfo=NULL;
}

static void sexypsf_xmms_pause(InputPlayback * playback, short p)
{
    if(!playing) return;
    playback->output->pause(p);
    paused = p;
}

static void sexypsf_xmms_seek(InputPlayback * data, int time)
{
    if(!playing) return;
    seek_time = time * 1000;
}

static int sexypsf_xmms_gettime(InputPlayback *playback)
{
    if(nextsong)
        return(-1);
    if(!playing) return(0);
    return playback->output->output_time();
}

static void sexypsf_xmms_getsonginfo(char *fn, char **title, int *length)
{
    PSFINFO *tmp;

    if((tmp=sexypsf_getpsfinfo(fn))) {
        *length = tmp->length;
        *title = get_title_psf(fn);
        sexypsf_freepsfinfo(tmp);
    }
}

static TitleInput *get_tuple_psf(gchar *fn) {
    TitleInput *tuple = NULL;
    PSFINFO *tmp = sexypsf_getpsfinfo(fn);

    if (tmp->length) {
        tuple = bmp_title_input_new();
        tuple->length = tmp->length;
        tuple->performer = g_strdup(tmp->artist);
        tuple->album_name = g_strdup(tmp->game);
        tuple->track_name = g_strdup(tmp->title);
        tuple->file_name = g_path_get_basename(fn);
        tuple->file_path = g_path_get_dirname(fn);
        sexypsf_freepsfinfo(tmp);
    }

    return tuple;
}   

static gchar *get_title_psf(gchar *fn) {
    gchar *title;
    TitleInput *tinput = get_tuple_psf(fn);

    if (tinput != NULL) {
        title = xmms_get_titlestring(xmms_get_gentitle_format(),
                                     tinput);
        bmp_title_input_free(tinput);
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

DECLARE_PLUGIN(sexypsf, NULL, NULL, sexypsf_iplist, NULL, NULL, NULL, NULL);
