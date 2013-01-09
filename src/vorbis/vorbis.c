/*
 * Copyright (C) Tony Arcieri <bascule@inferno.tusculum.edu>
 * Copyright (C) 2001-2002 Håvard Kvålen <havardk@xmms.org>
 * Copyright (C) 2007 William Pitcock <nenolod@sacredspiral.co.uk>
 * Copyright (C) 2008 Cristi Măgherușan <majeru@gentoo.ro>
 * Copyright (C) 2008 Eugene Zagidullin <e.asphyx@gmail.com>
 * Copyright (C) 2009-2011 Audacious Developers
 *
 * ReplayGain processing Copyright (C) 2002 Gian-Carlo Pascutto <gcp@sjeng.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "config.h"
/*#define AUD_DEBUG
#define DEBUG*/

#include <glib.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

#include "vorbis.h"

static size_t ovcb_read (void * buffer, size_t size, size_t count, void * file)
{
    return vfs_fread (buffer, size, count, file);
}

static int ovcb_seek (void * file, ogg_int64_t offset, int whence)
{
    return vfs_fseek (file, offset, whence);
}

static int ovcb_close (void * file)
{
    return 0;
}

static long ovcb_tell (void * file)
{
    return vfs_ftell (file);
}

ov_callbacks vorbis_callbacks = {
    ovcb_read,
    ovcb_seek,
    ovcb_close,
    ovcb_tell
};

ov_callbacks vorbis_callbacks_stream = {
    ovcb_read,
    NULL,
    ovcb_close,
    NULL
};

static gint seek_value;
static gboolean stop_flag = FALSE;
static pthread_mutex_t seek_mutex = PTHREAD_MUTEX_INITIALIZER;

static gint
vorbis_check_fd(const gchar *filename, VFSFile *stream)
{
    OggVorbis_File vfile;
    gint result;

    /*
     * The open function performs full stream detection and machine
     * initialization.  If it returns zero, the stream *is* Vorbis and
     * we're fully ready to decode.
     */

    memset(&vfile, 0, sizeof(vfile));

    result = ov_test_callbacks (stream, & vfile, NULL, 0, vfs_is_streaming
     (stream) ? vorbis_callbacks_stream : vorbis_callbacks);

    switch (result) {
    case OV_EREAD:
#ifdef DEBUG
        g_message("** vorbis.c: Media read error: %s", filename);
#endif
        return FALSE;
        break;
    case OV_ENOTVORBIS:
#ifdef DEBUG
        g_message("** vorbis.c: Not Vorbis data: %s", filename);
#endif
        return FALSE;
        break;
    case OV_EVERSION:
#ifdef DEBUG
        g_message("** vorbis.c: Version mismatch: %s", filename);
#endif
        return FALSE;
        break;
    case OV_EBADHEADER:
#ifdef DEBUG
        g_message("** vorbis.c: Invalid Vorbis bistream header: %s",
                  filename);
#endif
        return FALSE;
        break;
    case OV_EFAULT:
#ifdef DEBUG
        g_message("** vorbis.c: Internal logic fault while reading %s",
                  filename);
#endif
        return FALSE;
        break;
    case 0:
        break;
    default:
        break;
    }

    ov_clear(&vfile);
    return TRUE;
}

static void
set_tuple_str(Tuple *tuple, const gint nfield, const gchar *field,
    vorbis_comment *comment, gchar *key)
{
    gchar *str = vorbis_comment_query(comment, key, 0);
    if (str != NULL) {
        gchar *tmp = str_to_utf8(str);
        tuple_set_str(tuple, nfield, field, tmp);
        g_free(tmp);
    }
}

static Tuple *
get_tuple_for_vorbisfile(OggVorbis_File * vorbisfile, const gchar *filename)
{
    Tuple *tuple;
    gint length;
    vorbis_comment *comment = NULL;

    tuple = tuple_new_from_filename(filename);

    length = vfs_is_streaming (vorbisfile->datasource) ? -1 : ov_time_total
     (vorbisfile, -1) * 1000;

    /* associate with tuple */
    tuple_set_int(tuple, FIELD_LENGTH, NULL, length);

    if ((comment = ov_comment(vorbisfile, -1)) != NULL) {
        gchar *tmps;
        set_tuple_str(tuple, FIELD_TITLE, NULL, comment, "title");
        set_tuple_str(tuple, FIELD_ARTIST, NULL, comment, "artist");
        set_tuple_str(tuple, FIELD_ALBUM, NULL, comment, "album");
        set_tuple_str(tuple, FIELD_GENRE, NULL, comment, "genre");
        set_tuple_str(tuple, FIELD_COMMENT, NULL, comment, "comment");

        if ((tmps = vorbis_comment_query(comment, "tracknumber", 0)) != NULL)
            tuple_set_int(tuple, FIELD_TRACK_NUMBER, NULL, atoi(tmps));

        if ((tmps = vorbis_comment_query (comment, "date", 0)) != NULL)
            tuple_set_int (tuple, FIELD_YEAR, NULL, atoi (tmps));
    }

    vorbis_info * info = ov_info (vorbisfile, -1);
    tuple_set_format (tuple, "Ogg Vorbis", info->channels, info->rate,
     info->bitrate_nominal / 1000);

    tuple_set_str(tuple, FIELD_MIMETYPE, NULL, "application/ogg");

    return tuple;
}

static gfloat atof_no_locale (const gchar * string)
{
    gfloat result = 0;
    gboolean negative = FALSE;

    if (* string == '+')
        string ++;
    else if (* string == '-')
    {
        negative = TRUE;
        string ++;
    }

    while (* string >= '0' && * string <= '9')
        result = 10 * result + (* string ++ - '0');

    if (* string == '.')
    {
        gfloat place = 0.1;

        string ++;

        while (* string >= '0' && * string <= '9')
        {
            result += (* string ++ - '0') * place;
            place *= 0.1;
        }
    }

    return negative ? -result : result;
}

static gboolean
vorbis_update_replaygain(OggVorbis_File *vf, ReplayGainInfo *rg_info)
{
    vorbis_comment *comment;
    gchar *rg_gain, *rg_peak;

    if (vf == NULL || rg_info == NULL || (comment = ov_comment(vf, -1)) == NULL)
    {
#ifdef DEBUG
        printf ("No replay gain info.\n");
#endif
        return FALSE;
    }

    rg_gain = vorbis_comment_query(comment, "replaygain_album_gain", 0);
    if (!rg_gain) rg_gain = vorbis_comment_query(comment, "rg_audiophile", 0);    /* Old */
    rg_info->album_gain = (rg_gain != NULL) ? atof_no_locale (rg_gain) : 0.0;
#ifdef DEBUG
    printf ("Album gain: %s (%f)\n", rg_gain, rg_info->album_gain);
#endif

    rg_gain = vorbis_comment_query(comment, "replaygain_track_gain", 0);
    if (!rg_gain) rg_gain = vorbis_comment_query(comment, "rg_radio", 0);    /* Old */
    rg_info->track_gain = (rg_gain != NULL) ? atof_no_locale (rg_gain) : 0.0;
#ifdef DEBUG
    printf ("Track gain: %s (%f)\n", rg_gain, rg_info->track_gain);
#endif

    rg_peak = vorbis_comment_query(comment, "replaygain_album_peak", 0);
    rg_info->album_peak = rg_peak != NULL ? atof_no_locale (rg_peak) : 0.0;
#ifdef DEBUG
    printf ("Album peak: %s (%f)\n", rg_peak, rg_info->album_peak);
#endif

    rg_peak = vorbis_comment_query(comment, "replaygain_track_peak", 0);
    if (!rg_peak) rg_peak = vorbis_comment_query(comment, "rg_peak", 0);  /* Old */
    rg_info->track_peak = rg_peak != NULL ? atof_no_locale (rg_peak) : 0.0;
#ifdef DEBUG
    printf ("Track peak: %s (%f)\n", rg_peak, rg_info->track_peak);
#endif

    return TRUE;
}

static long
vorbis_interleave_buffer(float **pcm, int samples, int ch, float *pcmout)
{
    int i, j;
    for (i = 0; i < samples; i++)
        for (j = 0; j < ch; j++)
            *pcmout++ = pcm[j][i];

    return ch * samples * sizeof(float);
}


#define PCM_FRAMES 1024
#define PCM_BUFSIZE (PCM_FRAMES * 2)

static gboolean vorbis_play (InputPlayback * playback, const gchar * filename,
 VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
    if (file == NULL)
        return FALSE;

    vorbis_info *vi;
    OggVorbis_File vf;
    gint last_section = -1;
    ReplayGainInfo rg_info;
    gfloat pcmout[PCM_BUFSIZE*sizeof(float)], **pcm;
    gint bytes, channels, samplerate, br;
    gchar * title = NULL;

    seek_value = (start_time > 0) ? start_time : -1;
    stop_flag = FALSE;

    memset(&vf, 0, sizeof(vf));

    gboolean error = FALSE;

    if (ov_open_callbacks (file, & vf, NULL, 0, vfs_is_streaming (file) ?
     vorbis_callbacks_stream : vorbis_callbacks) < 0)
    {
        error = TRUE;
        goto play_cleanup;
    }

    vi = ov_info(&vf, -1);

    if (vi->channels > 2)
        goto play_cleanup;

    br = vi->bitrate_nominal;
    channels = vi->channels;
    samplerate = vi->rate;

    playback->set_params (playback, br, samplerate, channels);

    if (!playback->output->open_audio(FMT_FLOAT, samplerate, channels)) {
        error = TRUE;
        goto play_cleanup;
    }

    playback->output->flush (start_time);

    if (pause)
        playback->output->pause (TRUE);

    vorbis_update_replaygain(&vf, &rg_info);
    playback->output->set_replaygain_info (& rg_info);

    playback->set_pb_ready(playback);

    /*
     * Note that chaining changes things here; A vorbis file may
     * be a mix of different channels, bitrates and sample rates.
     * You can fetch the information for any section of the file
     * using the ov_ interface.
     */

    while (1)
    {
        if (stop_time >= 0 && playback->output->written_time () >= stop_time)
            goto DRAIN;

        pthread_mutex_lock (& seek_mutex);

        if (stop_flag)
        {
            pthread_mutex_unlock (& seek_mutex);
            break;
        }

        if (seek_value >= 0)
        {
            ov_time_seek (& vf, (double) seek_value / 1000);
            playback->output->flush (seek_value);
            seek_value = -1;
        }

        pthread_mutex_unlock (& seek_mutex);

        gint current_section = last_section;
        bytes = ov_read_float(&vf, &pcm, PCM_FRAMES, &current_section);
        if (bytes == OV_HOLE)
            continue;

        if (bytes <= 0)
        {
DRAIN:
            break;
        }

        bytes = vorbis_interleave_buffer (pcm, bytes, channels, pcmout);

        { /* try to detect when metadata has changed */
            vorbis_comment * comment = ov_comment (& vf, -1);
            const gchar * new_title = (comment == NULL) ? NULL :
             vorbis_comment_query (comment, "title", 0);

            if (new_title != NULL && (title == NULL || strcmp (title, new_title)))
            {
                g_free (title);
                title = g_strdup (new_title);

                playback->set_tuple (playback, get_tuple_for_vorbisfile (& vf,
                 filename));
            }
        }

        if (current_section != last_section)
        {
            /*
             * The info struct is different in each section.  vf
             * holds them all for the given bitstream.  This
             * requests the current one
             */
            vi = ov_info(&vf, -1);

            if (vi->channels > 2)
                goto stop_processing;

            if (vi->rate != samplerate || vi->channels != channels)
            {
                samplerate = vi->rate;
                channels = vi->channels;

                if (!playback->output->open_audio(FMT_FLOAT, vi->rate, vi->channels)) {
                    error = TRUE;
                    goto stop_processing;
                }

                playback->output->flush(ov_time_tell(&vf) * 1000);
                vorbis_update_replaygain(&vf, &rg_info);
                playback->output->set_replaygain_info (& rg_info); /* audio reopened */
            }
        }

        playback->output->write_audio (pcmout, bytes);

stop_processing:

        if (current_section != last_section)
        {
            playback->set_params (playback, br, samplerate, channels);
            last_section = current_section;
        }
    } /* main loop */

    pthread_mutex_lock (& seek_mutex);
    stop_flag = TRUE;
    pthread_mutex_unlock (& seek_mutex);

play_cleanup:

    ov_clear(&vf);
    g_free (title);
    return ! error;
}

static void vorbis_stop (InputPlayback * playback)
{
    pthread_mutex_lock (& seek_mutex);

    if (! stop_flag)
    {
        stop_flag = TRUE;
        playback->output->abort_write ();
    }

    pthread_mutex_unlock (& seek_mutex);
}

static void vorbis_pause (InputPlayback * playback, gboolean pause)
{
    pthread_mutex_lock (& seek_mutex);

    if (! stop_flag)
        playback->output->pause (pause);

    pthread_mutex_unlock (& seek_mutex);
}

static void vorbis_mseek (InputPlayback * playback, gint time)
{
    pthread_mutex_lock (& seek_mutex);

    if (! stop_flag)
    {
        seek_value = time;
        playback->output->abort_write ();
    }

    pthread_mutex_unlock (& seek_mutex);
}

static Tuple * get_song_tuple (const gchar * filename, VFSFile * file)
{
    OggVorbis_File vfile;          /* avoid thread interaction */
    Tuple *tuple = NULL;

    /*
     * The open function performs full stream detection and
     * machine initialization.  If it returns zero, the stream
     * *is* Vorbis and we're fully ready to decode.
     */
    if (ov_open_callbacks (file, & vfile, NULL, 0, vfs_is_streaming (file) ?
     vorbis_callbacks_stream : vorbis_callbacks) < 0)
        return NULL;

    tuple = get_tuple_for_vorbisfile(&vfile, filename);
    ov_clear(&vfile);
    return tuple;
}

static gboolean get_song_image (const gchar * filename, VFSFile * file,
 void * * data, gint64 * size)
{
    OggVorbis_File vfile;

    if (ov_open_callbacks (file, & vfile, NULL, 0, vfs_is_streaming (file) ?
     vorbis_callbacks_stream : vorbis_callbacks) < 0)
        return FALSE;

    vorbis_comment * comment = ov_comment (& vfile, -1);
    if (! comment)
        goto ERR;

    const gchar * s = vorbis_comment_query (comment, "METADATA_BLOCK_PICTURE", 0);
    if (! s)
        goto ERR;

    gsize length2;
    void * data2 = g_base64_decode (s, & length2);
    if (! data2 || length2 < 8)
        goto PARSE_ERR;

    gint mime_length = GUINT32_FROM_BE (* (guint32 *) (data2 + 4));
    if (length2 < 8 + mime_length + 4)
        goto PARSE_ERR;

    gint desc_length = GUINT32_FROM_BE (* (guint32 *) (data2 + 8 + mime_length));
    if (length2 < 8 + mime_length + 4 + desc_length + 20)
        goto PARSE_ERR;

    * size = GUINT32_FROM_BE (* (guint32 *) (data2 + 8 + mime_length + 4 + desc_length + 16));
    if (length2 < 8 + mime_length + 4 + desc_length + 20 + * size)
        goto PARSE_ERR;

    * data = g_malloc (* size);
    memcpy (* data, (char *) data2 + 8 + mime_length + 4 + desc_length + 20, * size);

    g_free (data2);
    ov_clear (& vfile);
    return TRUE;

PARSE_ERR:
    fprintf (stderr, "vorbis: Error parsing METADATA_BLOCK_PICTURE in %s.\n", filename);
    g_free (data2);

ERR:
    ov_clear (& vfile);
    return FALSE;
}

static const char vorbis_about[] =
 N_("Audacious Ogg Vorbis Decoder\n\n"
    "Based on the Xiph.org Foundation's Ogg Vorbis Plugin:\n"
    "http://www.xiph.org/\n\n"
    "Original code by:\n"
    "Tony Arcieri <bascule@inferno.tusculum.edu>\n\n"
    "Contributions from:\n"
    "Chris Montgomery <monty@xiph.org>\n"
    "Peter Alm <peter@xmms.org>\n"
    "Michael Smith <msmith@labyrinth.edu.au>\n"
    "Jack Moffitt <jack@icecast.org>\n"
    "Jorn Baayen <jorn@nl.linux.org>\n"
    "Håvard Kvålen <havardk@xmms.org>\n"
    "Gian-Carlo Pascutto <gcp@sjeng.org>\n"
    "Eugene Zagidullin <e.asphyx@gmail.com>");

static const gchar *vorbis_fmts[] = { "ogg", "ogm", "oga", NULL };
static const gchar * const mimes[] = {"application/ogg", NULL};

AUD_INPUT_PLUGIN
(
    .name = N_("Ogg Vorbis Decoder"),
    .domain = PACKAGE,
    .about_text = vorbis_about,
    .play = vorbis_play,
    .stop = vorbis_stop,
    .pause = vorbis_pause,
    .mseek = vorbis_mseek,
    .probe_for_tuple = get_song_tuple,
    .get_song_image = get_song_image,
    .update_song_tuple = vorbis_update_song_tuple,
    .is_our_file_from_vfs = vorbis_check_fd,
    .extensions = vorbis_fmts,
    .mimes = mimes,

    /* Vorbis probing is a bit slow; check for MP3 and AAC first. -jlindgren */
    .priority = 2,
)
