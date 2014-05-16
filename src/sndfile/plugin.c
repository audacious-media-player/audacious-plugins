/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011 Audacious development team
 *
 *  Based on the xmms_sndfile input plugin:
 *  Copyright (C) 2000, 2002 Erik de Castro Lopo
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

/*
 * Rewritten 17-Feb-2007 (nenolod):
 *   - now uses conditional variables to ensure that sndfile mutex is
 *     entirely protected.
 *   - pausing works now
 *   - fixed some potential race conditions when dealing with NFS.
 *   - TITLE_LEN removed
 *
 * Re-cleaned up 09-Aug-2009 (ccr):
 *   - removed threading madness.
 *   - improved locking.
 *   - misc. cleanups.
 *
 * Play loop rewritten 14-Nov-2009 (jlindgren):
 *   - decode in floating point
 *   - drain audio buffer before closing
 *   - handle seeking/stopping while paused
 */

#include <math.h>
#include <stdlib.h>

#include <glib.h>

#include <sndfile.h>

#include <audacious/input.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <libaudcore/audstrings.h>

/* Virtual file access wrappers for libsndfile
 */
static sf_count_t
sf_get_filelen (void *user_data)
{
    return vfs_fsize (user_data);
}

static sf_count_t
sf_vseek (sf_count_t offset, int whence, void *user_data)
{
    return vfs_fseek(user_data, offset, whence);
}

static sf_count_t
sf_vread (void *ptr, sf_count_t count, void *user_data)
{
    return vfs_fread(ptr, 1, count, user_data);
}

static sf_count_t
sf_vwrite (const void *ptr, sf_count_t count, void *user_data)
{
    return vfs_fwrite(ptr, 1, count, user_data);
}

static sf_count_t
sf_tell (void *user_data)
{
    return vfs_ftell(user_data);
}

static SF_VIRTUAL_IO sf_virtual_io =
{
    sf_get_filelen,
    sf_vseek,
    sf_vread,
    sf_vwrite,
    sf_tell
};

static void copy_string (SNDFILE * sf, int sf_id, Tuple * tup, int tup_id)
{
    const char * str = sf_get_string (sf, sf_id);
    if (str)
        tuple_set_str (tup, tup_id, str);
}

static void copy_int (SNDFILE * sf, int sf_id, Tuple * tup, int tup_id)
{
    const char * str = sf_get_string (sf, sf_id);
    if (str && atoi (str))
        tuple_set_int (tup, tup_id, atoi (str));
}

static Tuple * get_song_tuple (const char * filename, VFSFile * file)
{
    SNDFILE *sndfile;
    SF_INFO sfinfo;
    const char *format, *subformat;
    Tuple * ti;

    sndfile = sf_open_virtual (& sf_virtual_io, SFM_READ, & sfinfo, file);

    if (sndfile == NULL)
        return NULL;

    ti = tuple_new_from_filename (filename);

    copy_string (sndfile, SF_STR_TITLE, ti, FIELD_TITLE);
    copy_string (sndfile, SF_STR_ARTIST, ti, FIELD_ARTIST);
    copy_string (sndfile, SF_STR_ALBUM, ti, FIELD_ALBUM);
    copy_string (sndfile, SF_STR_COMMENT, ti, FIELD_COMMENT);
    copy_string (sndfile, SF_STR_GENRE, ti, FIELD_GENRE);
    copy_int (sndfile, SF_STR_DATE, ti, FIELD_YEAR);
    copy_int (sndfile, SF_STR_TRACKNUMBER, ti, FIELD_TRACK_NUMBER);

    sf_close (sndfile);

    if (sfinfo.samplerate > 0)
    {
        tuple_set_int(ti, FIELD_LENGTH,
        (int) ceil (1000.0 * sfinfo.frames / sfinfo.samplerate));
    }

    switch (sfinfo.format & SF_FORMAT_TYPEMASK)
    {
        case SF_FORMAT_WAV:
        case SF_FORMAT_WAVEX:
            format = "Microsoft WAV";
            break;
        case SF_FORMAT_AIFF:
            format = "Apple/SGI AIFF";
            break;
        case SF_FORMAT_AU:
            format = "Sun/NeXT AU";
            break;
        case SF_FORMAT_RAW:
            format = "Raw PCM data";
            break;
        case SF_FORMAT_PAF:
            format = "Ensoniq PARIS";
            break;
        case SF_FORMAT_SVX:
            format = "Amiga IFF / SVX8 / SV16";
            break;
        case SF_FORMAT_NIST:
            format = "Sphere NIST";
            break;
        case SF_FORMAT_VOC:
            format = "Creative VOC";
            break;
        case SF_FORMAT_IRCAM:
            format = "Berkeley/IRCAM/CARL";
            break;
        case SF_FORMAT_W64:
            format = "Sonic Foundry's 64 bit RIFF/WAV";
            break;
        case SF_FORMAT_MAT4:
            format = "Matlab (tm) V4.2 / GNU Octave 2.0";
            break;
        case SF_FORMAT_MAT5:
            format = "Matlab (tm) V5.0 / GNU Octave 2.1";
            break;
        case SF_FORMAT_PVF:
            format = "Portable Voice Format";
            break;
        case SF_FORMAT_XI:
            format = "Fasttracker 2 Extended Instrument";
            break;
        case SF_FORMAT_HTK:
            format = "HMM Tool Kit";
            break;
        case SF_FORMAT_SDS:
            format = "Midi Sample Dump Standard";
            break;
        case SF_FORMAT_AVR:
            format = "Audio Visual Research";
            break;
        case SF_FORMAT_SD2:
            format = "Sound Designer 2";
            break;
        case SF_FORMAT_FLAC:
            format = "Free Lossless Audio Codec";
            break;
        case SF_FORMAT_CAF:
            format = "Core Audio File";
            break;
        default:
            format = "Unknown sndfile";
    }

    switch (sfinfo.format & SF_FORMAT_SUBMASK)
    {
        case SF_FORMAT_PCM_S8:
            subformat = "signed 8 bit";
            break;
        case SF_FORMAT_PCM_16:
            subformat = "signed 16 bit";
            break;
        case SF_FORMAT_PCM_24:
            subformat = "signed 24 bit";
            break;
        case SF_FORMAT_PCM_32:
            subformat = "signed 32 bit";
            break;
        case SF_FORMAT_PCM_U8:
            subformat = "unsigned 8 bit";
            break;
        case SF_FORMAT_FLOAT:
            subformat = "32 bit float";
            break;
        case SF_FORMAT_DOUBLE:
            subformat = "64 bit float";
            break;
        case SF_FORMAT_ULAW:
            subformat = "U-Law";
            break;
        case SF_FORMAT_ALAW:
            subformat = "A-Law";
            break;
        case SF_FORMAT_IMA_ADPCM:
            subformat = "IMA ADPCM";
            break;
        case SF_FORMAT_MS_ADPCM:
            subformat = "MS ADPCM";
            break;
        case SF_FORMAT_GSM610:
            subformat = "GSM 6.10";
            break;
        case SF_FORMAT_VOX_ADPCM:
            subformat = "Oki Dialogic ADPCM";
            break;
        case SF_FORMAT_G721_32:
            subformat = "32kbs G721 ADPCM";
            break;
        case SF_FORMAT_G723_24:
            subformat = "24kbs G723 ADPCM";
            break;
        case SF_FORMAT_G723_40:
            subformat = "40kbs G723 ADPCM";
            break;
        case SF_FORMAT_DWVW_12:
            subformat = "12 bit Delta Width Variable Word";
            break;
        case SF_FORMAT_DWVW_16:
            subformat = "16 bit Delta Width Variable Word";
            break;
        case SF_FORMAT_DWVW_24:
            subformat = "24 bit Delta Width Variable Word";
            break;
        case SF_FORMAT_DWVW_N:
            subformat = "N bit Delta Width Variable Word";
            break;
        case SF_FORMAT_DPCM_8:
            subformat = "8 bit differential PCM";
            break;
        case SF_FORMAT_DPCM_16:
            subformat = "16 bit differential PCM";
            break;
        default:
            subformat = NULL;
    }

    if (subformat != NULL)
    {
        SPRINTF (codec, "%s (%s)", format, subformat);
        tuple_set_format (ti, codec, sfinfo.channels, sfinfo.samplerate, 0);
    }
    else
        tuple_set_format (ti, format, sfinfo.channels, sfinfo.samplerate, 0);

    return ti;
}

static bool_t play_start (const char * filename, VFSFile * file)
{
    if (file == NULL)
        return FALSE;

    SF_INFO sfinfo;
    SNDFILE * sndfile = sf_open_virtual (& sf_virtual_io, SFM_READ, & sfinfo,
     file);
    if (sndfile == NULL)
        return FALSE;

    if (! aud_input_open_audio (FMT_FLOAT, sfinfo.samplerate,
     sfinfo.channels))
    {
        sf_close (sndfile);
        return FALSE;
    }

    int size = sfinfo.channels * (sfinfo.samplerate / 50);
    float * buffer = g_new (float, size);

    while (! aud_input_check_stop ())
    {
        int seek_value = aud_input_check_seek ();
        if (seek_value != -1)
            sf_seek (sndfile, (int64_t) seek_value * sfinfo.samplerate / 1000, SEEK_SET);

        int samples = sf_read_float (sndfile, buffer, size);
        if (! samples)
            break;

        aud_input_write_audio (buffer, sizeof (float) * samples);
    }

    sf_close (sndfile);
    g_free (buffer);

    return TRUE;
}

static int
is_our_file_from_vfs(const char *filename, VFSFile *fin)
{
    SNDFILE *tmp_sndfile;
    SF_INFO tmp_sfinfo;

    /* Have to open the file to see if libsndfile can handle it. */
    tmp_sndfile = sf_open_virtual (&sf_virtual_io, SFM_READ, &tmp_sfinfo, fin);

    if (!tmp_sndfile)
        return FALSE;

    /* It can so close file and return TRUE. */
    sf_close (tmp_sndfile);
    tmp_sndfile = NULL;

    return TRUE;
}

const char plugin_about[] =
 N_("Based on the xmms_sndfile plugin:\n"
    "Copyright (C) 2000, 2002 Erik de Castro Lopo\n\n"
    "Adapted for Audacious by Tony Vroon <chainsaw@gentoo.org>\n\n"
    "This program is free software; you can redistribute it and/or "
    "modify it under the terms of the GNU General Public License "
    "as published by the Free Software Foundation; either version 2 "
    "of the License, or (at your option) any later version.\n\n"
    "This program is distributed in the hope that it will be useful, "
    "but WITHOUT ANY WARRANTY; without even the implied warranty of "
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
    "GNU General Public License for more details.\n\n"
    "You should have received a copy of the GNU General Public License "
    "along with this program; if not, write to the Free Software "
    "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.");

static const char *sndfile_fmts[] = { "aiff", "au", "raw", "wav", NULL };

AUD_INPUT_PLUGIN
(
    .name = N_("Sndfile Plugin"),
    .domain = PACKAGE,
    .about_text = plugin_about,
    .play = play_start,
    .probe_for_tuple = get_song_tuple,
    .is_our_file_from_vfs = is_our_file_from_vfs,
    .extensions = sndfile_fmts,

    /* low priority fallback (but before ffaudio) */
    .priority = 9,
)
