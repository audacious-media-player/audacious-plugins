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

/*#define AUD_DEBUG
#define DEBUG*/

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <libaudcore/i18n.h>
#include <libaudcore/input.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>

#include "vorbis.h"

static size_t ovcb_read (void * buffer, size_t size, size_t count, void * file)
{
    return vfs_fread (buffer, size, count, (VFSFile *) file);
}

static int ovcb_seek (void * file, ogg_int64_t offset, int whence)
{
    return vfs_fseek ((VFSFile *) file, offset, whence);
}

static int ovcb_close (void * file)
{
    return 0;
}

static long ovcb_tell (void * file)
{
    return vfs_ftell ((VFSFile *) file);
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

static bool_t vorbis_check_fd (const char * filename, VFSFile * file)
{
    ogg_sync_state oy = {0};
    ogg_stream_state os = {0};
    ogg_page og = {0};
    ogg_packet op = {0};

    bool_t result = FALSE;

    ogg_sync_init (& oy);

    while (1)
    {
        int64_t bytes = ogg_sync_pageseek (& oy, & og);

        if (bytes < 0) /* skipped some bytes */
            continue;
        if (bytes > 0) /* got a page */
            break;

        void * buffer = ogg_sync_buffer (& oy, 2048);
        bytes = vfs_fread (buffer, 1, 2048, file);

        if (bytes <= 0)
            goto end;

        ogg_sync_wrote (& oy, bytes);
    }

    if (! ogg_page_bos (& og))
        goto end;

    ogg_stream_init (& os, ogg_page_serialno (& og));
    ogg_stream_pagein (& os, & og);

    if (ogg_stream_packetout (& os, & op) > 0 && vorbis_synthesis_idheader (& op))
        result = TRUE;

end:
    ogg_sync_clear (& oy);
    ogg_stream_clear (& os);

    return result;
}

static void
set_tuple_str(Tuple *tuple, const gint nfield,
    vorbis_comment *comment, const gchar *key)
{
    tuple_set_str (tuple, nfield, vorbis_comment_query (comment, key, 0));
}

static Tuple *
get_tuple_for_vorbisfile(OggVorbis_File * vorbisfile, const gchar *filename)
{
    Tuple *tuple;
    gint length = -1;
    vorbis_comment *comment = NULL;

    tuple = tuple_new_from_filename(filename);

    if (! vfs_is_streaming ((VFSFile *) vorbisfile->datasource))
        length = ov_time_total (vorbisfile, -1) * 1000;

    /* associate with tuple */
    tuple_set_int(tuple, FIELD_LENGTH, length);

    if ((comment = ov_comment(vorbisfile, -1)) != NULL) {
        gchar *tmps;
        set_tuple_str(tuple, FIELD_TITLE, comment, "title");
        set_tuple_str(tuple, FIELD_ARTIST, comment, "artist");
        set_tuple_str(tuple, FIELD_ALBUM, comment, "album");
        set_tuple_str(tuple, FIELD_GENRE, comment, "genre");
        set_tuple_str(tuple, FIELD_COMMENT, comment, "comment");

        if ((tmps = vorbis_comment_query(comment, "tracknumber", 0)) != NULL)
            tuple_set_int(tuple, FIELD_TRACK_NUMBER, atoi(tmps));

        if ((tmps = vorbis_comment_query (comment, "date", 0)) != NULL)
            tuple_set_int (tuple, FIELD_YEAR, atoi (tmps));
    }

    vorbis_info * info = ov_info (vorbisfile, -1);
    tuple_set_format (tuple, "Ogg Vorbis", info->channels, info->rate,
     info->bitrate_nominal / 1000);

    tuple_set_str(tuple, FIELD_MIMETYPE, "application/ogg");

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

static gboolean vorbis_play (const gchar * filename, VFSFile * file)
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

    aud_input_set_bitrate (br);

    if (!aud_input_open_audio(FMT_FLOAT, samplerate, channels)) {
        error = TRUE;
        goto play_cleanup;
    }

    vorbis_update_replaygain(&vf, &rg_info);
    aud_input_set_gain (& rg_info);

    /*
     * Note that chaining changes things here; A vorbis file may
     * be a mix of different channels, bitrates and sample rates.
     * You can fetch the information for any section of the file
     * using the ov_ interface.
     */

    while (! aud_input_check_stop ())
    {
        int seek_value = aud_input_check_seek();

        if (seek_value >= 0 && ov_time_seek (& vf, (double) seek_value / 1000) < 0)
        {
            fprintf (stderr, "vorbis: seek failed\n");
            error = TRUE;
            break;
        }

        gint current_section = last_section;
        bytes = ov_read_float(&vf, &pcm, PCM_FRAMES, &current_section);
        if (bytes == OV_HOLE)
            continue;

        if (bytes <= 0)
            break;

        bytes = vorbis_interleave_buffer (pcm, bytes, channels, pcmout);

        { /* try to detect when metadata has changed */
            vorbis_comment * comment = ov_comment (& vf, -1);
            const gchar * new_title = (comment == NULL) ? NULL :
             vorbis_comment_query (comment, "title", 0);

            if (new_title != NULL && (title == NULL || strcmp (title, new_title)))
            {
                g_free (title);
                title = g_strdup (new_title);

                aud_input_set_tuple (get_tuple_for_vorbisfile (& vf,
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

                if (!aud_input_open_audio(FMT_FLOAT, vi->rate, vi->channels)) {
                    error = TRUE;
                    goto stop_processing;
                }

                vorbis_update_replaygain(&vf, &rg_info);
                aud_input_set_gain (& rg_info); /* audio reopened */
            }
        }

        aud_input_write_audio (pcmout, bytes);

stop_processing:

        if (current_section != last_section)
        {
            aud_input_set_bitrate (br);
            last_section = current_section;
        }
    } /* main loop */

play_cleanup:

    ov_clear(&vf);
    g_free (title);
    return ! error;
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

    const gchar * s;

    if ((s = vorbis_comment_query (comment, "METADATA_BLOCK_PICTURE", 0)))
    {
        unsigned mime_length, desc_length;

        gsize length2;
        unsigned char * data2 = g_base64_decode (s, & length2);
        if (! data2 || length2 < 8)
            goto PARSE_ERR;

        mime_length = GUINT32_FROM_BE (* (guint32 *) (data2 + 4));
        if (length2 < 8 + mime_length + 4)
            goto PARSE_ERR;

        desc_length = GUINT32_FROM_BE (* (guint32 *) (data2 + 8 + mime_length));
        if (length2 < 8 + mime_length + 4 + desc_length + 20)
            goto PARSE_ERR;

        * size = GUINT32_FROM_BE (* (guint32 *) (data2 + 8 + mime_length + 4 + desc_length + 16));
        if (length2 < 8 + mime_length + 4 + desc_length + 20 + (unsigned) (* size))
            goto PARSE_ERR;

        * data = g_memdup ((char *) data2 + 8 + mime_length + 4 + desc_length + 20, * size);

        g_free (data2);
        ov_clear (& vfile);
        return TRUE;

    PARSE_ERR:
        fprintf (stderr, "vorbis: Error parsing METADATA_BLOCK_PICTURE in %s.\n", filename);
        g_free (data2);
    }

    if ((s = vorbis_comment_query (comment, "COVERART", 0)))
    {
        gsize length2;
        void * data2 = g_base64_decode (s, & length2);

        if (! data2 || ! length2)
        {
            fprintf (stderr, "vorbis: Error parsing COVERART in %s.\n", filename);
            g_free (data2);
            goto ERR;
        }

        * data = data2;
        * size = length2;

        ov_clear (& vfile);
        return TRUE;
    }

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

#define AUD_PLUGIN_NAME        N_("Ogg Vorbis Decoder")
#define AUD_PLUGIN_ABOUT       vorbis_about
#define AUD_INPUT_PLAY         vorbis_play
#define AUD_INPUT_READ_TUPLE   get_song_tuple
#define AUD_INPUT_READ_IMAGE   get_song_image
#define AUD_INPUT_WRITE_TUPLE  vorbis_update_song_tuple
#define AUD_INPUT_IS_OUR_FILE  vorbis_check_fd
#define AUD_INPUT_EXTS         vorbis_fmts
#define AUD_INPUT_MIMES        mimes

/* medium-high priority (a little slow) */
#define AUD_INPUT_PRIORITY     2

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
