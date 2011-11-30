/*
 *  Copyright 2000 Martin Strau? <mys@faveve.uni-stuttgart.de>
 *
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

#include "config.h"

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include <glib.h>
#include <string.h>

#define MIN_BPM         1
#define MAX_BPM         512
#define TACT_ID_MAX     12
#define TACT_FORM_MAX   8

#define AUDIO_FREQ      (44100)
#define BUF_SAMPLES     512
#define BUF_BYTES       (BUF_SAMPLES * 2)
#define MAX_AMPL        (GINT16_TO_LE((1 << 15) - 1))


typedef struct
{
    gint bpm;
    gint num;
    gint den;
    gint id;
} metronom_t;

gint tact_id[TACT_ID_MAX][2] = {
    {1, 1},
    {2, 2},
    {3, 2},
    {4, 2},
    {2, 4},
    {3, 4},
    {4, 4},
    {6, 4},
    {2, 8},
    {3, 8},
    {4, 8},
    {6, 8}
};

gdouble tact_form[TACT_ID_MAX][TACT_FORM_MAX] = {
    {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.6, 0.5, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.6, 0.5, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.5, 0.6, 0.5, 0.5, 0.0, 0.0},
    {1.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.6, 0.5, 0.0, 0.0, 0.0, 0.0},
    {1.0, 0.5, 0.5, 0.6, 0.5, 0.5, 0.0, 0.0}
};

static gboolean stop_flag = FALSE;

static void metronom_about (void)
{
    static GtkWidget * aboutbox = NULL;

    audgui_simple_message (& aboutbox, GTK_MESSAGE_INFO, _("About Metronom"),
     _("A Tact Generator by Martin Strauss <mys@faveve.uni-stuttgart.de>\n\n"
     "To use it, add a URL: tact://beats*num/den\n"
     "e.g. tact://77 to play 77 beats per minute\n"
     "or   tact://60*3/4 to play 60 bpm in 3/4 tacts"));
}

static gboolean metronom_is_our_fd(const gchar * filename, VFSFile *fd)
{
    if (!strncmp(filename, "tact://", 7))
        return TRUE;
    return FALSE;
}

static gboolean metronom_get_cp(const gchar *filename, metronom_t *pmetronom, gchar **str)
{
    gsize count;

    count = sscanf(filename, "tact://%d*%d/%d",
        &pmetronom->bpm, &pmetronom->num, &pmetronom->den);

    if (count != 1 && count != 3)
        return FALSE;

    if (pmetronom->bpm < MIN_BPM || pmetronom->bpm > MAX_BPM)
        return FALSE;

    if (count == 1)
    {
        pmetronom->num = 1;
        pmetronom->den = 1;
        pmetronom->id = 0;
    }
    else
    {
        gboolean flag;
        gint id;

        if (pmetronom->num == 0 || pmetronom->den == 0)
            return FALSE;

        flag = FALSE;
        for (id = 0; id < TACT_ID_MAX && !flag; id++)
        {
            if (pmetronom->num == tact_id[id][0] && pmetronom->den == tact_id[id][1])
                flag = TRUE;
        }

        if (!flag)
            return FALSE;
        else
            pmetronom->id = id;
    }

    if (str == NULL)
        return TRUE;

    if (pmetronom->num == 1 && pmetronom->den == 1)
        *str = g_strdup_printf(_("Tact generator: %d bpm"), pmetronom->bpm);
    else
        *str = g_strdup_printf(_("Tact generator: %d bpm %d/%d"), pmetronom->bpm, pmetronom->num, pmetronom->den);

    return TRUE;
}

static gboolean metronom_play(InputPlayback *playback, const gchar *filename,
    VFSFile *file, gint start_time, gint stop_time, gboolean pause)
{
    metronom_t pmetronom;
    gint16 data[BUF_SAMPLES];
    gint t = 0, tact, num;
    gint datagoal = 0;
    gint datamiddle = 0;
    gint datacurrent = datamiddle;
    gint datalast = datamiddle;
    gint data_form[TACT_FORM_MAX];
    gboolean error = FALSE;

    if (playback->output->open_audio(FMT_S16_LE, AUDIO_FREQ, 1) == 0)
    {
        error = TRUE;
        goto error_exit;
    }

    if (!metronom_get_cp(filename, &pmetronom, NULL))
    {
        g_message("Invalid metronom tact parameters in URI %s", filename);
        goto error_exit;
    }

    if (pause)
        playback->output->pause(TRUE);

    playback->set_params(playback, sizeof(data[0]) * 8 * AUDIO_FREQ, AUDIO_FREQ, 1);

    tact = 60 * AUDIO_FREQ / pmetronom.bpm;

    /* prepare weighted amplitudes */
    for (num = 0; num < pmetronom.num; num++)
    {
        data_form[num] = MAX_AMPL * tact_form[pmetronom.id][num];
    }

    stop_flag = FALSE;
    playback->set_pb_ready(playback);

    num = 0;
    while (!stop_flag)
    {
        gint i;

        for (i = 0; i < BUF_SAMPLES; i++)
        {
            if (t == tact)
            {
                t = 0;
                datagoal = data_form[num];
            }
            else if (t == 10)
            {
                datagoal = -data_form[num];
            }
            else if (t == 25)
            {
                datagoal = data_form[num];
                /* circle through weighted amplitudes */
                num++;
                if (num >= pmetronom.num)
                    num = 0;
            }
            /* makes curve a little bit smoother  */
            data[i] = (datalast + datacurrent + datagoal) / 3;
            datalast = datacurrent;
            datacurrent = data[i];
            if (t > 35)
                datagoal = (datamiddle + 7 * datagoal) / 8;
            t++;
        }

        if (!stop_flag)
            playback->output->write_audio(data, BUF_BYTES);
    }

error_exit:

    stop_flag = TRUE;
    playback->output->close_audio();

    return !error;
}

static void metronom_stop(InputPlayback * playback)
{
    stop_flag = TRUE;
    playback->output->abort_write();
}

static void metronom_pause(InputPlayback * playback, gboolean pause)
{
    if (!stop_flag)
        playback->output->pause(pause);
}

static Tuple *metronom_probe_for_tuple(const gchar * filename, VFSFile *fd)
{
    Tuple *tuple = tuple_new_from_filename(filename);
    metronom_t metronom;
    gchar *tmp = NULL;

    if (metronom_get_cp(filename, &metronom, &tmp))
        tuple_associate_string(tuple, FIELD_TITLE, NULL, tmp);

    g_free(tmp);

    return tuple;
}

static const gchar * const schemes[] = {"tact", NULL};

AUD_INPUT_PLUGIN
(
    .name = "Tact Generator",
    .schemes = schemes,
    .about = metronom_about,
    .is_our_file_from_vfs = metronom_is_our_fd,
    .play = metronom_play,
    .stop = metronom_stop,
    .pause = metronom_pause,
    .probe_for_tuple = metronom_probe_for_tuple,
)
