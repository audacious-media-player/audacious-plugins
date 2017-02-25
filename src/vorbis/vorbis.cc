/*
 * Copyright (C) Tony Arcieri <bascule@inferno.tusculum.edu>
 * Copyright (C) 2001-2002 Håvard Kvålen <havardk@xmms.org>
 * Copyright (C) 2007 William Pitcock <nenolod@sacredspiral.co.uk>
 * Copyright (C) 2008 Cristi Măgherușan <majeru@gentoo.ro>
 * Copyright (C) 2008 Eugene Zagidullin <e.asphyx@gmail.com>
 * Copyright (C) 2009-2015 Audacious Developers
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

#define AUD_GLIB_INTEGRATION
#define WANT_AUD_BSWAP
#define WANT_VFS_STDIO_COMPAT
#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>

#include "vorbis.h"

EXPORT VorbisPlugin aud_plugin_instance;

static Index<char> read_image_from_comment (const char * filename, vorbis_comment * comment);

static size_t ovcb_read (void * buffer, size_t size, size_t count, void * file)
{
    return ((VFSFile *) file)->fread (buffer, size, count);
}

static int ovcb_seek (void * file, ogg_int64_t offset, int whence)
{
    return ((VFSFile *) file)->fseek (offset, to_vfs_seek_type (whence));
}

static int ovcb_close (void * file)
{
    return 0;
}

static long ovcb_tell (void * file)
{
    return ((VFSFile *) file)->ftell ();
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

bool VorbisPlugin::is_our_file (const char * filename, VFSFile & file)
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
        bytes = file.fread (buffer, 1, 2048);

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
set_tuple_str(Tuple &tuple, Tuple::Field field,
    vorbis_comment *comment, const char *key)
{
    tuple.set_str (field, vorbis_comment_query (comment, key, 0));
}

static void read_comment (vorbis_comment * comment, Tuple & tuple)
{
    const char * tmps;

    set_tuple_str (tuple, Tuple::Title, comment, "TITLE");
    set_tuple_str (tuple, Tuple::Artist, comment, "ARTIST");
    set_tuple_str (tuple, Tuple::Album, comment, "ALBUM");
    set_tuple_str (tuple, Tuple::AlbumArtist, comment, "ALBUMARTIST");
    set_tuple_str (tuple, Tuple::Genre, comment, "GENRE");
    set_tuple_str (tuple, Tuple::Comment, comment, "COMMENT");

    if ((tmps = vorbis_comment_query (comment, "TRACKNUMBER", 0)))
        tuple.set_int (Tuple::Track, atoi (tmps));
    if ((tmps = vorbis_comment_query (comment, "DATE", 0)))
        tuple.set_int (Tuple::Year, atoi (tmps));
}

/* try to detect when metadata has changed */
static bool update_tuple (OggVorbis_File * vf, Tuple & tuple)
{
    vorbis_comment * comment = ov_comment (vf, -1);
    if (! comment)
        return false;

    String old_title = tuple.get_str (Tuple::Title);
    const char * new_title = vorbis_comment_query (comment, "TITLE", 0);

    if (! new_title || (old_title && ! strcmp (old_title, new_title)))
        return false;

    read_comment (comment, tuple);
    return true;
}

static bool update_replay_gain (OggVorbis_File * vf, ReplayGainInfo * rg_info)
{
    const char *rg_gain, *rg_peak;

    vorbis_comment * comment = ov_comment (vf, -1);
    if (! comment)
        return false;

    rg_gain = vorbis_comment_query(comment, "REPLAYGAIN_ALBUM_GAIN", 0);
    if (!rg_gain) rg_gain = vorbis_comment_query(comment, "RG_AUDIOPHILE", 0);    /* Old */
    rg_info->album_gain = (rg_gain != nullptr) ? str_to_double (rg_gain) : 0.0;
    AUDDBG ("Album gain: %s (%f)\n", rg_gain, rg_info->album_gain);

    rg_gain = vorbis_comment_query(comment, "REPLAYGAIN_TRACK_GAIN", 0);
    if (!rg_gain) rg_gain = vorbis_comment_query(comment, "RG_RADIO", 0);    /* Old */
    rg_info->track_gain = (rg_gain != nullptr) ? str_to_double (rg_gain) : 0.0;
    AUDDBG ("Track gain: %s (%f)\n", rg_gain, rg_info->track_gain);

    rg_peak = vorbis_comment_query(comment, "REPLAYGAIN_ALBUM_PEAK", 0);
    rg_info->album_peak = rg_peak != nullptr ? str_to_double (rg_peak) : 0.0;
    AUDDBG ("Album peak: %s (%f)\n", rg_peak, rg_info->album_peak);

    rg_peak = vorbis_comment_query(comment, "REPLAYGAIN_TRACK_PEAK", 0);
    if (!rg_peak) rg_peak = vorbis_comment_query(comment, "RG_PEAK", 0);  /* Old */
    rg_info->track_peak = rg_peak != nullptr ? str_to_double (rg_peak) : 0.0;
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

bool VorbisPlugin::play (const char * filename, VFSFile & file)
{
    vorbis_info *vi;
    OggVorbis_File vf;
    int last_section = -1;
    Tuple tuple = get_playback_tuple ();
    ReplayGainInfo rg_info;
    float pcmout[PCM_BUFSIZE*sizeof(float)], **pcm;
    int bytes, channels, samplerate, br;

    memset(&vf, 0, sizeof(vf));

    bool stream = (file.fsize () < 0);
    bool error = false;

    if (ov_open_callbacks (& file, & vf, nullptr, 0, stream ?
     vorbis_callbacks_stream : vorbis_callbacks) < 0)
    {
        error = true;
        goto play_cleanup;
    }

    vi = ov_info(&vf, -1);

    br = vi->bitrate_nominal;
    channels = vi->channels;
    samplerate = vi->rate;

    set_stream_bitrate (br);

    if (update_tuple (& vf, tuple))
        set_playback_tuple (tuple.ref ());

    if (update_replay_gain (& vf, & rg_info))
        set_replay_gain (rg_info);

    open_audio (FMT_FLOAT, samplerate, channels);

    /*
     * Note that chaining changes things here; A vorbis file may
     * be a mix of different channels, bitrates and sample rates.
     * You can fetch the information for any section of the file
     * using the ov_ interface.
     */

    while (! check_stop ())
    {
        int seek_value = check_seek ();

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

        if (update_tuple (& vf, tuple))
            set_playback_tuple (tuple.ref ());

        if (current_section != last_section)
        {
            /*
             * The info struct is different in each section.  vf
             * holds them all for the given bitstream.  This
             * requests the current one
             */
            vi = ov_info(&vf, -1);

            if (vi->rate != samplerate || vi->channels != channels)
            {
                samplerate = vi->rate;
                channels = vi->channels;

                if (update_replay_gain (& vf, & rg_info))
                    set_replay_gain (rg_info);

                open_audio (FMT_FLOAT, vi->rate, vi->channels);
            }
        }

        write_audio (pcmout, bytes);

        if (current_section != last_section)
        {
            set_stream_bitrate (br);
            last_section = current_section;
        }
    } /* main loop */

play_cleanup:

    ov_clear(&vf);
    return ! error;
}

bool VorbisPlugin::read_tag (const char * filename, VFSFile & file,
 Tuple & tuple, Index<char> * image)
{
    OggVorbis_File vfile;

    bool stream = (file.fsize () < 0);

    /*
     * The open function performs full stream detection and
     * machine initialization.  If it returns zero, the stream
     * *is* Vorbis and we're fully ready to decode.
     */
    if (ov_open_callbacks (& file, & vfile, nullptr, 0, stream ?
     vorbis_callbacks_stream : vorbis_callbacks) < 0)
        return false;

    vorbis_info * info = ov_info (& vfile, -1);
    vorbis_comment * comment = ov_comment (& vfile, -1);

    tuple.set_format ("Ogg Vorbis", info->channels, info->rate, info->bitrate_nominal / 1000);

    if (! stream)
        tuple.set_int (Tuple::Length, ov_time_total (& vfile, -1) * 1000);

    if (comment)
        read_comment (comment, tuple);

    if (image && comment)
        * image = read_image_from_comment (filename, comment);

    ov_clear (& vfile);
    return true;
}

static Index<char> read_image_from_comment (const char * filename, vorbis_comment * comment)
{
    Index<char> data;
    const char * s;

    if ((s = vorbis_comment_query (comment, "METADATA_BLOCK_PICTURE", 0)))
    {
        unsigned mime_length, desc_length, length;

        size_t length2;
        CharPtr data2 ((char *) g_base64_decode (s, & length2));
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

        data.insert (data2 + 8 + mime_length + 4 + desc_length + 20, 0, length);
        return data;

    PARSE_ERR:
        AUDERR ("Error parsing METADATA_BLOCK_PICTURE in %s.\n", filename);
    }

    if ((s = vorbis_comment_query (comment, "COVERART", 0)))
    {
        size_t length2;
        CharPtr data2 ((char *) g_base64_decode (s, & length2));

        if (data2 && length2)
            data.insert (data2, 0, length2);
        else
            AUDERR ("Error parsing COVERART in %s.\n", filename);

        return data;
    }

    return data;
}

const char VorbisPlugin::about[] =
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

const char * const VorbisPlugin::exts[] = {"ogg", "ogm", "oga", nullptr};
const char * const VorbisPlugin::mimes[] = {"application/ogg", nullptr};
