/*  VTXplugin - VTX player for XMMS
 *
 *  Copyright (C) 2002-2004 Sashnov Alexander
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/tuple_formatter.h>

#include "vtx.h"
#include "ayemu.h"

extern InputPlugin vtx_ip;

#define SNDBUFSIZE 1024
char sndbuf[SNDBUFSIZE];

static GThread *play_thread = NULL;

int seek_to;

int freq = 44100;
int chans = 2;

/* NOTE: if you change 'bits' you also need change constant FMT_S16_NE
 * to more appropriate in line
 *
 * (vtx_ip.output->open_audio (FMT_S16_NE, * freq, chans) == 0)
 * in function vtx_play_file()
 *
 * and in produce_audio() function call.
 */
const int bits = 16;

ayemu_ay_t ay;
ayemu_vtx_t vtx;

static const gchar *vtx_fmts[] = { "vtx", NULL };

static gboolean vtx_init(void)
{
    mcs_handle_t *db;
    db = aud_cfg_db_open();

    aud_cfg_db_get_int(db, NULL, "src_rate", &freq);
    if (freq < 4000 || freq > 192000)
        freq = 44100;

    aud_cfg_db_close(db);
    return TRUE;
}

gint vtx_is_our_fd(const gchar * filename, VFSFile * fp)
{
    char buf[2];

    vfs_fread(buf, 2, 1, fp);
    return (!strncasecmp(buf, "ay", 2) || !strncasecmp(buf, "ym", 2));
}

gint vtx_is_our_file(const gchar * filename)
{
    gboolean ret;
    VFSFile *fp;

    fp = vfs_fopen(filename, "rb");
    ret = vtx_is_our_fd(filename, fp);
    vfs_fclose(fp);

    return ret;
}

Tuple *vtx_get_song_tuple_from_vtx(const gchar * filename, ayemu_vtx_t * in)
{
    Tuple *out = tuple_new_from_filename(filename);

    tuple_associate_string(out, FIELD_ARTIST, NULL, in->hdr.author);
    tuple_associate_string(out, FIELD_TITLE, NULL, in->hdr.title);

    tuple_associate_int(out, FIELD_LENGTH, NULL, in->hdr.regdata_size / 14 * 1000 / 50);

    tuple_associate_string(out, FIELD_GENRE, NULL, (in->hdr.chiptype == AYEMU_AY) ? "AY chiptunes" : "YM chiptunes");
    tuple_associate_string(out, FIELD_ALBUM, NULL, in->hdr.from);
    tuple_associate_string(out, -1, "game", in->hdr.from);

    tuple_associate_string(out, FIELD_QUALITY, NULL, "sequenced");
    tuple_associate_string(out, FIELD_CODEC, NULL, in->hdr.tracker);
    tuple_associate_string(out, -1, "tracker", in->hdr.tracker);

    tuple_associate_int(out, FIELD_YEAR, NULL, in->hdr.year);

    return out;
}

Tuple *vtx_get_song_tuple(const gchar * filename)
{
    ayemu_vtx_t tmp;

    if (ayemu_vtx_open(&tmp, filename))
    {
        Tuple *ti = vtx_get_song_tuple_from_vtx(filename, &tmp);
        ayemu_vtx_free(&tmp);
        return ti;
    }

    return NULL;
}

/* sound playing thread, runing by vtx_play_file() */
static gpointer play_loop(gpointer args)
{
    InputPlayback *playback = (InputPlayback *) args;
    void *stream;               /* pointer to current position in sound buffer */
    unsigned char regs[14];
    int need;
    int left;                   /* how many sound frames can play with current AY register frame */
    int donow;
    int rate;

    left = 0;
    rate = chans * (bits / 8);

    while (playback->playing && !playback->eof)
    {
        /* fill sound buffer */
        stream = sndbuf;
        for (need = SNDBUFSIZE / rate; need > 0; need -= donow)
            if (left > 0)
            {                   /* use current AY register frame */
                donow = (need > left) ? left : need;
                left -= donow;
                stream = ayemu_gen_sound(&ay, (char *)stream, donow * rate);
            }
            else
            {                   /* get next AY register frame */
                if (ayemu_vtx_get_next_frame(&vtx, (char *)regs) == 0)
                {
                    playback->eof = TRUE;
                    donow = need;
                    memset(stream, 0, donow * rate);
                }
                else
                {
                    left = freq / vtx.hdr.playerFreq;
                    ayemu_set_regs(&ay, regs);
                    donow = 0;
                }
            }

        if (playback->playing && seek_to == -1)
            playback->pass_audio(playback, FMT_S16_NE, chans, SNDBUFSIZE, sndbuf, &playback->playing);

        if (playback->eof)
        {
            while (playback->output->buffer_playing())
                g_usleep(10000);
            playback->playing = 0;
        }

        /* jump to time in seek_to (in seconds) */
        if (seek_to != -1)
        {
            vtx.pos = seek_to * 50;     /* (time in sec) * 50 = offset in AY register data frames */
            playback->output->flush(seek_to * 1000);
            seek_to = -1;
        }
    }
    ayemu_vtx_free(&vtx);
    return NULL;
}

void vtx_play_file(InputPlayback * playback)
{
    gchar *filename = playback->filename;
    gchar *buf;
    Tuple *ti;

    memset(&ay, 0, sizeof(ay));

    if (!ayemu_vtx_open(&vtx, filename))
        g_print("libvtx: Error read vtx header from %s\n", filename);
    else if (!ayemu_vtx_load_data(&vtx))
        g_print("libvtx: Error read vtx data from %s\n", filename);
    else
    {
        ayemu_init(&ay);
        ayemu_set_chip_type(&ay, vtx.hdr.chiptype, NULL);
        ayemu_set_chip_freq(&ay, vtx.hdr.chipFreq);
        ayemu_set_stereo(&ay, vtx.hdr.stereo, NULL);

        playback->error = FALSE;
        if (playback->output->open_audio(FMT_S16_NE, freq, chans) == 0)
        {
            g_print("libvtx: output audio error!\n");
            playback->error = TRUE;
            playback->playing = FALSE;
            return;
        }

        playback->eof = FALSE;
        seek_to = -1;

        ti = vtx_get_song_tuple_from_vtx(playback->filename, &vtx);
        buf = tuple_formatter_make_title_string(ti, aud_get_gentitle_format());

        playback->set_params(playback, buf, vtx.hdr.regdata_size / 14 * 1000 / 50, 14 * 50 * 8, freq, bits / 8);

        g_free(buf);

        tuple_free(ti);

        playback->playing = TRUE;
        play_thread = g_thread_self();
        playback->set_pb_ready(playback);
        play_loop(playback);
    }
}

void vtx_stop(InputPlayback * playback)
{
    if (playback->playing && play_thread != NULL)
    {
        playback->playing = FALSE;

        g_thread_join(play_thread);
        play_thread = NULL;
        playback->output->close_audio();
        ayemu_vtx_free(&vtx);
    }
}

/* seek to specified number of seconds */
void vtx_seek(InputPlayback * playback, gint time)
{
    if (time * 50 < vtx.hdr.regdata_size / 14)
    {
        playback->eof = FALSE;
        seek_to = time;

        /* wait for affect changes in parallel thread */
        while (seek_to != -1)
            g_usleep(10000);
    }
}

/* Pause or unpause */
void vtx_pause(InputPlayback * playback, gshort p)
{
    playback->output->pause(p);
}

InputPlugin vtx_ip = {
    .description = "VTX Audio Plugin",  /* Plugin description */
    .init = vtx_init,           /* Initialization */
    .about = vtx_about,         /* Show aboutbox */
    .configure = NULL,
    .is_our_file = vtx_is_our_file,     /* Check file, return 1 if the plugin can handle this file */
    .play_file = vtx_play_file, /* Play given file */
    .stop = vtx_stop,           /* Stop playing */
    .pause = vtx_pause,         /* Pause playing */
    .seek = vtx_seek,           /* Seek time */
    .file_info_box = vtx_file_info,     /* Show file-information dialog */
    .get_song_tuple = vtx_get_song_tuple,       /* Tuple */
    .is_our_file_from_vfs = vtx_is_our_fd,      /* VFS */
    .vfs_extensions = vtx_fmts  /* ext assist */
};

InputPlugin *vtx_iplist[] = { &vtx_ip, NULL };

SIMPLE_INPUT_PLUGIN(vtx, vtx_iplist);
