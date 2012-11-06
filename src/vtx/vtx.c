/*  VTX Input Plugin for Audacious
 *
 *  Copyright (C) 2002-2004 Sashnov Alexander
 *  Copyright (C) 2010 Michał Lipski <tallica@o2.pl>
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

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/debug.h>

#include "vtx.h"
#include "ayemu.h"

#define SNDBUFSIZE 1024
static gchar sndbuf[SNDBUFSIZE];
static gint freq = 44100;
static gint chans = 2;
static gint bits = 16;

static GMutex *seek_mutex;
static GCond *seek_cond;
static gint seek_value;
static gboolean stop_flag = FALSE;

ayemu_ay_t ay;
ayemu_vtx_t vtx;

static const gchar *vtx_fmts[] = { "vtx", NULL };

static gboolean vtx_init(void)
{
    seek_mutex = g_mutex_new();
    seek_cond = g_cond_new();

    return TRUE;
}

static void vtx_cleanup(void)
{
    g_mutex_free(seek_mutex);
    g_cond_free(seek_cond);
}

gint vtx_is_our_fd(const gchar * filename, VFSFile * fp)
{
    gchar buf[2];
    if (vfs_fread(buf, 1, 2, fp) < 2)
        return FALSE;
    return (!strncasecmp(buf, "ay", 2) || !strncasecmp(buf, "ym", 2));
}

Tuple *vtx_get_song_tuple_from_vtx(const gchar * filename, ayemu_vtx_t * in)
{
    Tuple *out = tuple_new_from_filename(filename);

    tuple_set_str(out, FIELD_ARTIST, NULL, in->hdr.author);
    tuple_set_str(out, FIELD_TITLE, NULL, in->hdr.title);

    tuple_set_int(out, FIELD_LENGTH, NULL, in->hdr.regdata_size / 14 * 1000 / 50);

    tuple_set_str(out, FIELD_GENRE, NULL, (in->hdr.chiptype == AYEMU_AY) ? "AY chiptunes" : "YM chiptunes");
    tuple_set_str(out, FIELD_ALBUM, NULL, in->hdr.from);
    tuple_set_str(out, -1, "game", in->hdr.from);

    tuple_set_str(out, FIELD_QUALITY, NULL, _("sequenced"));
    tuple_set_str(out, FIELD_CODEC, NULL, in->hdr.tracker);
    tuple_set_str(out, -1, "tracker", in->hdr.tracker);

    tuple_set_int(out, FIELD_YEAR, NULL, in->hdr.year);

    return out;
}

Tuple *vtx_probe_for_tuple(const gchar *filename, VFSFile *fd)
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

static gboolean vtx_play(InputPlayback * playback, const gchar * filename,
 VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
    gboolean error = FALSE;
    gboolean eof = FALSE;
    void *stream;               /* pointer to current position in sound buffer */
    guchar regs[14];
    gint need;
    gint left;                   /* how many sound frames can play with current AY register frame */
    gint donow;
    gint rate;

    left = 0;
    rate = chans * (bits / 8);

    memset(&ay, 0, sizeof(ay));

    if (!ayemu_vtx_open(&vtx, filename))
    {
        g_print("libvtx: Error read vtx header from %s\n", filename);
        error = TRUE;
        goto ERR_NO_CLOSE;
    }
    else if (!ayemu_vtx_load_data(&vtx))
    {
        g_print("libvtx: Error read vtx data from %s\n", filename);
        error = TRUE;
        goto ERR_NO_CLOSE;
    }

    ayemu_init(&ay);
    ayemu_set_chip_type(&ay, vtx.hdr.chiptype, NULL);
    ayemu_set_chip_freq(&ay, vtx.hdr.chipFreq);
    ayemu_set_stereo(&ay, vtx.hdr.stereo, NULL);

    if (playback->output->open_audio(FMT_S16_NE, freq, chans) == 0)
    {
        g_print("libvtx: output audio error!\n");
        error = TRUE;
        goto ERR_NO_CLOSE;
    }

    if (pause)
        playback->output->pause (TRUE);

    stop_flag = FALSE;

    playback->set_params(playback, 14 * 50 * 8, freq, bits / 8);
    playback->set_pb_ready(playback);

    while (!stop_flag)
    {
        g_mutex_lock(seek_mutex);

        if (seek_value >= 0)
        {
            vtx.pos = seek_value * 50 / 1000;     /* (time in sec) * 50 = offset in AY register data frames */
            playback->output->flush(seek_value);
            seek_value = -1;
            g_cond_signal(seek_cond);
        }

        g_mutex_unlock(seek_mutex);

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
                    donow = need;
                    memset(stream, 0, donow * rate);
                    eof = TRUE;
                }
                else
                {
                    left = freq / vtx.hdr.playerFreq;
                    ayemu_set_regs(&ay, regs);
                    donow = 0;
                }
            }

        if (!stop_flag)
            playback->output->write_audio(sndbuf, SNDBUFSIZE);

        if (eof)
            goto CLEANUP;
    }

CLEANUP:
    ayemu_vtx_free(&vtx);

    g_mutex_lock(seek_mutex);
    stop_flag = TRUE;
    g_cond_signal(seek_cond); /* wake up any waiting request */
    g_mutex_unlock(seek_mutex);

ERR_NO_CLOSE:

    return !error;
}

void vtx_stop(InputPlayback * playback)
{
    g_mutex_lock(seek_mutex);

    if (!stop_flag)
    {
        stop_flag = TRUE;
        playback->output->abort_write();
        g_cond_signal(seek_cond);
    }

    g_mutex_unlock (seek_mutex);
}

void vtx_seek(InputPlayback * playback, gint time)
{
    g_mutex_lock(seek_mutex);

    if (!stop_flag)
    {
        seek_value = time;
        playback->output->abort_write();
        g_cond_signal(seek_cond);
        g_cond_wait(seek_cond, seek_mutex);
    }

    g_mutex_unlock(seek_mutex);
}

void vtx_pause(InputPlayback * playback, gboolean pause)
{
    g_mutex_lock(seek_mutex);

    if (!stop_flag)
        playback->output->pause(pause);

    g_mutex_unlock(seek_mutex);
}

static const char vtx_about[] =
 N_("Vortex file format player by Sashnov Alexander <sashnov@ngs.ru>\n"
    "Based on in_vtx.dll by Roman Sherbakov <v_soft@microfor.ru>\n"
    "Audacious plugin by Pavel Vymetalek <pvymetalek@seznam.cz>");

AUD_INPUT_PLUGIN
(
    .name = N_("VTX Decoder"),
    .domain = PACKAGE,
    .about_text = vtx_about,
    .init = vtx_init,
    .cleanup = vtx_cleanup,
    .play = vtx_play,
    .stop = vtx_stop,
    .pause = vtx_pause,
    .mseek = vtx_seek,
    .file_info_box = vtx_file_info,
    .probe_for_tuple = vtx_probe_for_tuple,
    .is_our_file_from_vfs = vtx_is_our_fd,
    .extensions = vtx_fmts
)
