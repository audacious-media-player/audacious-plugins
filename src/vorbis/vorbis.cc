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

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#define WANT_AUD_BSWAP
#define WANT_VFS_STDIO_COMPAT
#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/input.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#include "vorbis.h"

static size_t ovcb_read (void * buffer, size_t size, size_t count, void * file)
{
    return vfs_fread (buffer, size, count, (VFSFile *) file);
}

static int ovcb_seek (void * file, ogg_int64_t offset, int whence)
{
    return vfs_fseek ((VFSFile *) file, offset, to_vfs_seek_type (whence));
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
    nullptr,
    ovcb_close,
    nullptr
};

static bool vorbis_check_fd (const char * filename, VFSFile * file)
{
    ogg_sync_state oy = {0};
    ogg_stream_state os = {0};
    ogg_page og = {0};
    ogg_packet op = {0};

    bool result = false;

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
        result = true;

end:
    ogg_sync_clear (& oy);
    ogg_stream_clear (& os);

    return result;
}

static void
set_tuple_str(Tuple &tuple, const int nfield,
    vorbis_comment *comment, const char *key)
{
    tuple.set_str (nfield, vorbis_comment_query (comment, key, 0));
}

static Tuple
get_tuple_for_vorbisfile(OggVorbis_File * vorbisfile, const char *filename)
{
    Tuple tuple;
    int length = -1;
    vorbis_comment *comment = nullptr;

    tuple.set_filename(filename);

    if (! vfs_is_streaming ((VFSFile *) vorbisfile->datasource))
        length = ov_time_total (vorbisfile, -1) * 1000;

    /* associate with tuple */
    tuple.set_int (FIELD_LENGTH, length);

    if ((comment = ov_comment(vorbisfile, -1)) != nullptr) {
        char *tmps;
        set_tuple_str(tuple, FIELD_TITLE, comment, "title");
        set_tuple_str(tuple, FIELD_ARTIST, comment, "artist");
        set_tuple_str(tuple, FIELD_ALBUM, comment, "album");
        set_tuple_str(tuple, FIELD_GENRE, comment, "genre");
        set_tuple_str(tuple, FIELD_COMMENT, comment, "comment");

        if ((tmps = vorbis_comment_query(comment, "tracknumber", 0)) != nullptr)
            tuple.set_int (FIELD_TRACK_NUMBER, atoi(tmps));

        if ((tmps = vorbis_comment_query (comment, "date", 0)) != nullptr)
            tuple.set_int (FIELD_YEAR, atoi (tmps));
    }

    vorbis_info * info = ov_info (vorbisfile, -1);
    tuple.set_format ("Ogg Vorbis", info->channels, info->rate, info->bitrate_nominal / 1000);

    tuple.set_str (FIELD_MIMETYPE, "application/ogg");

    return tuple;
}

static float atof_no_locale (const char * string)
{
    float result = 0;
    bool negative = false;

    if (* string == '+')
        string ++;
    else if (* string == '-')
    {
        negative = true;
        string ++;
    }

    while (* string >= '0' && * string <= '9')
        result = 10 * result + (* string ++ - '0');

    if (* string == '.')
    {
        float place = 0.1;

        string ++;

        while (* string >= '0' && * string <= '9')
        {
            result += (* string ++ - '0') * place;
            place *= 0.1;
        }
    }

    return negative ? -result : result;
}

static bool
vorbis_update_replaygain(OggVorbis_File *vf, ReplayGainInfo *rg_info)
{
    vorbis_comment *comment;
    char *rg_gain, *rg_peak;

    if (vf == nullptr || rg_info == nullptr || (comment = ov_comment(vf, -1)) == nullptr)
    {
        AUDDBG ("No replay gain info.\n");
        return false;
    }

    rg_gain = vorbis_comment_query(comment, "replaygain_album_gain", 0);
    if (!rg_gain) rg_gain = vorbis_comment_query(comment, "rg_audiophile", 0);    /* Old */
    rg_info->album_gain = (rg_gain != nullptr) ? atof_no_locale (rg_gain) : 0.0;
    AUDDBG ("Album gain: %s (%f)\n", rg_gain, rg_info->album_gain);

    rg_gain = vorbis_comment_query(comment, "replaygain_track_gain", 0);
    if (!rg_gain) rg_gain = vorbis_comment_query(comment, "rg_radio", 0);    /* Old */
    rg_info->track_gain = (rg_gain != nullptr) ? atof_no_locale (rg_gain) : 0.0;
    AUDDBG ("Track gain: %s (%f)\n", rg_gain, rg_info->track_gain);

    rg_peak = vorbis_comment_query(comment, "replaygain_album_peak", 0);
    rg_info->album_peak = rg_peak != nullptr ? atof_no_locale (rg_peak) : 0.0;
    AUDDBG ("Album peak: %s (%f)\n", rg_peak, rg_info->album_peak);

    rg_peak = vorbis_comment_query(comment, "replaygain_track_peak", 0);
    if (!rg_peak) rg_peak = vorbis_comment_query(comment, "rg_peak", 0);  /* Old */
    rg_info->track_peak = rg_peak != nullptr ? atof_no_locale (rg_peak) : 0.0;
    AUDDBG ("Track peak: %s (%f)\n", rg_peak, rg_info->track_peak);

    return true;
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

static bool vorbis_play (const char * filename, VFSFile * file)
{
    if (file == nullptr)
        return false;

    vorbis_info *vi;
    OggVorbis_File vf;
    int last_section = -1;
    ReplayGainInfo rg_info;
    float pcmout[PCM_BUFSIZE*sizeof(float)], **pcm;
    int bytes, channels, samplerate, br;
    char * title = nullptr;

    memset(&vf, 0, sizeof(vf));

    bool error = false;

    if (ov_open_callbacks (file, & vf, nullptr, 0, vfs_is_streaming (file) ?
     vorbis_callbacks_stream : vorbis_callbacks) < 0)
    {
        error = true;
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
        error = true;
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
            AUDERR ("seek failed\n");
            error = true;
            break;
        }

        int current_section = last_section;
        bytes = ov_read_float(&vf, &pcm, PCM_FRAMES, &current_section);
        if (bytes == OV_HOLE)
            continue;

        if (bytes <= 0)
            break;

        bytes = vorbis_interleave_buffer (pcm, bytes, channels, pcmout);

        { /* try to detect when metadata has changed */
            vorbis_comment * comment = ov_comment (& vf, -1);
            const char * new_title = (comment == nullptr) ? nullptr :
             vorbis_comment_query (comment, "title", 0);

            if (new_title != nullptr && (title == nullptr || strcmp (title, new_title)))
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
                    error = true;
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

static Tuple get_song_tuple (const char * filename, VFSFile * file)
{
    OggVorbis_File vfile;          /* avoid thread interaction */

    /*
     * The open function performs full stream detection and
     * machine initialization.  If it returns zero, the stream
     * *is* Vorbis and we're fully ready to decode.
     */
    if (ov_open_callbacks (file, & vfile, nullptr, 0, vfs_is_streaming (file) ?
     vorbis_callbacks_stream : vorbis_callbacks) < 0)
        return Tuple ();

    Tuple tuple = get_tuple_for_vorbisfile(&vfile, filename);
    ov_clear(&vfile);
    return tuple;
}

static Index<char> get_song_image (const char * filename, VFSFile * file)
{
    Index<char> data;

    OggVorbis_File vfile;

    if (ov_open_callbacks (file, & vfile, nullptr, 0, vfs_is_streaming (file) ?
     vorbis_callbacks_stream : vorbis_callbacks) < 0)
        return data;

    vorbis_comment * comment = ov_comment (& vfile, -1);
    if (! comment)
        goto ERR;

    const char * s;

    if ((s = vorbis_comment_query (comment, "METADATA_BLOCK_PICTURE", 0)))
    {
        unsigned mime_length, desc_length, length;

        size_t length2;
        unsigned char * data2 = g_base64_decode (s, & length2);
        if (! data2 || length2 < 8)
            goto PARSE_ERR;

        mime_length = FROM_BE32 (* (uint32_t *) (data2 + 4));
        if (length2 < 8 + mime_length + 4)
            goto PARSE_ERR;

        desc_length = FROM_BE32 (* (uint32_t *) (data2 + 8 + mime_length));
        if (length2 < 8 + mime_length + 4 + desc_length + 20)
            goto PARSE_ERR;

        length = FROM_BE32 (* (uint32_t *) (data2 + 8 + mime_length + 4 + desc_length + 16));
        if (length2 < 8 + mime_length + 4 + desc_length + 20 + length)
            goto PARSE_ERR;

        data.insert ((char *) data2 + 8 + mime_length + 4 + desc_length + 20, 0, length);

        g_free (data2);
        ov_clear (& vfile);
        return data;

    PARSE_ERR:
        AUDERR ("Error parsing METADATA_BLOCK_PICTURE in %s.\n", filename);
        g_free (data2);
    }

    if ((s = vorbis_comment_query (comment, "COVERART", 0)))
    {
        size_t length2;
        void * data2 = g_base64_decode (s, & length2);

        if (! data2 || ! length2)
        {
            AUDERR ("Error parsing COVERART in %s.\n", filename);
            g_free (data2);
            goto ERR;
        }

        data.insert ((const char *) data2, 0, length2);

        g_free (data2);
        ov_clear (& vfile);
        return data;
    }

ERR:
    ov_clear (& vfile);
    return data;
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

static const char *vorbis_fmts[] = { "ogg", "ogm", "oga", nullptr };
static const char * const mimes[] = {"application/ogg", nullptr};

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
