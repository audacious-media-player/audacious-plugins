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

#define WANT_VFS_STDIO_COMPAT
#include <libaudcore/input.h>
#include <libaudcore/plugin.h>
#include <libaudcore/i18n.h>
#include <libaudcore/audstrings.h>

/* Virtual file access wrappers for libsndfile
 */
static sf_count_t
sf_get_filelen (void *user_data)
{
    return ((VFSFile *) user_data)->fsize ();
}

static sf_count_t
sf_vseek (sf_count_t offset, int whence, void *user_data)
{
    return ((VFSFile *) user_data)->fseek (offset, to_vfs_seek_type (whence));
}

static sf_count_t
sf_vread (void *ptr, sf_count_t count, void *user_data)
{
    return ((VFSFile *) user_data)->fread (ptr, 1, count);
}

static sf_count_t
sf_vwrite (const void *ptr, sf_count_t count, void *user_data)
{
    return ((VFSFile *) user_data)->fwrite (ptr, 1, count);
}

static sf_count_t
sf_tell (void *user_data)
{
    return ((VFSFile *) user_data)->ftell ();
}

static SF_VIRTUAL_IO sf_virtual_io =
{
    sf_get_filelen,
    sf_vseek,
    sf_vread,
    sf_vwrite,
    sf_tell
};

static void copy_string (SNDFILE * sf, int sf_id, Tuple & tup, Tuple::Field field)
{
    const char * str = sf_get_string (sf, sf_id);
    if (str)
        tup.set_str (field, str);
}

static void copy_int (SNDFILE * sf, int sf_id, Tuple & tup, Tuple::Field field)
{
    const char * str = sf_get_string (sf, sf_id);
    if (str && atoi (str))
        tup.set_int (field, atoi (str));
}

static Tuple get_song_tuple (const char * filename, VFSFile & file)
{
    SNDFILE *sndfile;
    SF_INFO sfinfo;
    const char *format, *subformat;
    Tuple ti;

    sndfile = sf_open_virtual (& sf_virtual_io, SFM_READ, & sfinfo, & file);

    if (sndfile == nullptr)
        return ti;

    ti.set_filename (filename);

    copy_string (sndfile, SF_STR_TITLE, ti, Tuple::Title);
    copy_string (sndfile, SF_STR_ARTIST, ti, Tuple::Artist);
    copy_string (sndfile, SF_STR_ALBUM, ti, Tuple::Album);
    copy_string (sndfile, SF_STR_COMMENT, ti, Tuple::Comment);
    copy_string (sndfile, SF_STR_GENRE, ti, Tuple::Genre);
    copy_int (sndfile, SF_STR_DATE, ti, Tuple::Year);
    copy_int (sndfile, SF_STR_TRACKNUMBER, ti, Tuple::Track);

    sf_close (sndfile);

    if (sfinfo.samplerate > 0)
        ti.set_int (Tuple::Length, ceil (1000.0 * sfinfo.frames / sfinfo.samplerate));

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
            subformat = nullptr;
    }

    if (subformat != nullptr)
        ti.set_format (str_printf ("%s (%s)", format, subformat),
         sfinfo.channels, sfinfo.samplerate, 0);
    else
        ti.set_format (format, sfinfo.channels, sfinfo.samplerate, 0);

    return ti;
}

static bool play_start (const char * filename, VFSFile & file)
{
    SF_INFO sfinfo;
    SNDFILE * sndfile = sf_open_virtual (& sf_virtual_io, SFM_READ, & sfinfo, & file);
    if (sndfile == nullptr)
        return false;

    if (! aud_input_open_audio (FMT_FLOAT, sfinfo.samplerate,
     sfinfo.channels))
    {
        sf_close (sndfile);
        return false;
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

    return true;
}

static bool
is_our_file_from_vfs(const char *filename, VFSFile &fin)
{
    SNDFILE *tmp_sndfile;
    SF_INFO tmp_sfinfo;

    /* Have to open the file to see if libsndfile can handle it. */
    tmp_sndfile = sf_open_virtual (&sf_virtual_io, SFM_READ, &tmp_sfinfo, &fin);

    if (!tmp_sndfile)
        return false;

    /* It can so close file and return true. */
    sf_close (tmp_sndfile);
    tmp_sndfile = nullptr;

    return true;
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

static const char *sndfile_fmts[] = { "aiff", "au", "raw", "wav", nullptr };

#define AUD_PLUGIN_NAME        N_("Sndfile Plugin")
#define AUD_PLUGIN_ABOUT       plugin_about
#define AUD_INPUT_PLAY         play_start
#define AUD_INPUT_READ_TUPLE   get_song_tuple
#define AUD_INPUT_IS_OUR_FILE  is_our_file_from_vfs
#define AUD_INPUT_EXTS         sndfile_fmts

/* low priority fallback (but before ffaudio) */
#define AUD_INPUT_PRIORITY     9

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
