/*  FileWriter-Plugin
 *  (C) copyright 2007 merging of Disk Writer and Out-Lame by Michael Färber
 *
 *  Original Out-Lame-Plugin:
 *  (C) copyright 2002 Lars Siebold <khandha5@gmx.net>
 *  (C) copyright 2006-2007 porting to audacious by Yoshiki Yazawa <yaz@cc.rim.or.jp>
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

/* #define AUD_DEBUG 1 */

#include "filewriter.h"

#ifdef FILEWRITER_MP3

#include <lame/lame.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>

static lame_global_flags *gfp;
static unsigned char encbuffer[LAME_MAXMP3BUFFER];
static int id3v2_size;

static int channels;
static unsigned long numsamples;
static Index<unsigned char> write_buffer;

static void lame_debugf(const char *format, va_list ap)
{
    (void) vprintf(format, ap);
}

static const char * const mp3_defaults[] = {
 "vbr_on", "0",
 "vbr_type", "0",
 "vbr_min_val", "32",
 "vbr_max_val", "320",
 "enforce_min_val", "0",
 "vbr_quality_val", "4",
 "abr_val", "128",
 "toggle_xing_val", "1",
 "mark_original_val", "1",
 "mark_copyright_val", "0",
 "force_v2_val", "0",
 "only_v1_val", "0",
 "only_v2_val", "0",
 "algo_quality_val", "5",
 "out_samplerate_val", "0",
 "bitrate_val", "128",
 "compression_val", "11",
 "enc_toggle_val", "0",
 "audio_mode_val", aud::numeric_string<NOT_SET>::str,
 "enforce_iso_val", "0",
 "error_protect_val", "0",
 nullptr};

#define GET_INT(n)       aud_get_int("filewriter_mp3", n)
#define GET_DOUBLE(n)    aud_get_double("filewriter_mp3", n)

static void mp3_init ()
{
    aud_config_set_defaults ("filewriter_mp3", mp3_defaults);
}

static bool mp3_open (VFSFile & file, const format_info & info, const Tuple & tuple)
{
    int imp3;

    gfp = lame_init();
    if (gfp == nullptr)
        return false;

    /* setup id3 data */
    id3tag_init(gfp);

    /* XXX write UTF-8 even though libmp3lame does id3v2.3. --yaz */
    id3tag_set_title(gfp, tuple.get_str (Tuple::Title));
    id3tag_set_artist(gfp, tuple.get_str (Tuple::Artist));
    id3tag_set_album(gfp, tuple.get_str (Tuple::Album));
    id3tag_set_genre(gfp, tuple.get_str (Tuple::Genre));
    id3tag_set_year(gfp, int_to_str (tuple.get_int (Tuple::Year)));
    id3tag_set_track(gfp, int_to_str (tuple.get_int (Tuple::Track)));

    if (GET_INT("force_v2_val"))
        id3tag_add_v2(gfp);
    if (GET_INT("only_v1_val"))
        id3tag_v1_only(gfp);
    if (GET_INT("only_v2_val"))
        id3tag_v2_only(gfp);

    /* input stream description */

    lame_set_in_samplerate(gfp, info.frequency);
    lame_set_num_channels(gfp, info.channels);
    lame_set_out_samplerate(gfp, GET_INT("out_samplerate_val"));

    /* general control parameters */

    lame_set_bWriteVbrTag(gfp, GET_INT("toggle_xing_val"));
    lame_set_quality(gfp, GET_INT("algo_quality_val"));

    const int audio_mode_val = GET_INT("audio_mode_val");
    if (audio_mode_val != NOT_SET) {
        AUDDBG("set mode to %d\n", audio_mode_val);
        lame_set_mode(gfp, (MPEG_mode) audio_mode_val);
    }

    lame_set_errorf(gfp, lame_debugf);
    lame_set_debugf(gfp, lame_debugf);
    lame_set_msgf(gfp, lame_debugf);

    const int vbr_on = GET_INT("vbr_on");
    if (vbr_on == 0) {
        if (GET_INT("enc_toggle_val") == 0)
            lame_set_brate(gfp, GET_INT("bitrate_val"));
        else
            lame_set_compression_ratio(gfp, GET_DOUBLE("compression_val"));
    }

    /* frame params */

    lame_set_copyright(gfp, GET_INT("mark_copyright_val"));
    lame_set_original(gfp, GET_INT("mark_original_val"));
    lame_set_error_protection(gfp, GET_INT("error_protect_val"));
    lame_set_strict_ISO(gfp, GET_INT("enforce_iso_val"));

    if (vbr_on != 0) {
        const int vbr_min_val = GET_INT("vbr_min_val");
        const int vbr_max_val = GET_INT("vbr_max_val");

        if (GET_INT("vbr_type") == 0)
            lame_set_VBR(gfp, vbr_rh);
        else
            lame_set_VBR(gfp, vbr_abr);

        lame_set_VBR_q(gfp, GET_INT("vbr_quality_val"));
        lame_set_VBR_mean_bitrate_kbps(gfp, GET_INT("abr_val"));
        lame_set_VBR_min_bitrate_kbps(gfp, vbr_min_val);
        lame_set_VBR_max_bitrate_kbps(gfp, aud::max(vbr_min_val, vbr_max_val));
        lame_set_VBR_hard_min(gfp, GET_INT("enforce_min_val"));
    }

    /* not to write id3 tag automatically. */
    lame_set_write_id3tag_automatic(gfp, 0);

    if (lame_init_params(gfp) == -1)
        return false;

    /* write id3v2 header */
    imp3 = lame_get_id3v2_tag(gfp, encbuffer, sizeof(encbuffer));

    if (imp3 > 0) {
        if (file.fwrite (encbuffer, 1, imp3) != imp3)
            AUDERR ("write error\n");
        id3v2_size = imp3;
    }
    else {
        id3v2_size = 0;
    }

    channels = info.channels;
    numsamples = 0;
    return true;
}

static void mp3_write (VFSFile & file, const void * data, int length)
{
    int encoded;

    if (! write_buffer.len ())
        write_buffer.resize (8192);

    while (1)
    {
        if (channels == 1)
            encoded = lame_encode_buffer_ieee_float (gfp, (const float *) data,
             (const float *) data, length / sizeof (float),
             write_buffer.begin (), write_buffer.len ());
        else
            encoded = lame_encode_buffer_interleaved_ieee_float (gfp,
             (const float *) data, length / (2 * sizeof (float)),
             write_buffer.begin (), write_buffer.len ());

        if (encoded != -1)
            break;

        write_buffer.resize (write_buffer.len () * 2);
    }

    if (encoded > 0 && file.fwrite (write_buffer.begin (), 1, encoded) != encoded)
        AUDERR ("write error\n");

    numsamples += length / (2 * channels);
}

static void mp3_close (VFSFile & file)
{
    int imp3, encout;

    /* write remaining mp3 data */
    encout = lame_encode_flush_nogap(gfp, encbuffer, LAME_MAXMP3BUFFER);
    if (file.fwrite (encbuffer, 1, encout) != encout)
        AUDERR ("write error\n");

    /* set gfp->num_samples for valid TLEN tag */
    lame_set_num_samples(gfp, numsamples);

    /* append v1 tag */
    imp3 = lame_get_id3v1_tag(gfp, encbuffer, sizeof(encbuffer));
    if (imp3 > 0 && file.fwrite (encbuffer, 1, imp3) != imp3)
        AUDERR ("write error\n");

    /* update v2 tag */
    imp3 = lame_get_id3v2_tag(gfp, encbuffer, sizeof(encbuffer));
    if (imp3 > 0) {
        if (file.fseek (0, VFS_SEEK_SET) != 0)
            AUDERR ("seek error\n");
        else if (file.fwrite (encbuffer, 1, imp3) != imp3)
            AUDERR ("write error\n");
    }

    /* update lame tag */
    if (id3v2_size) {
        if (file.fseek (id3v2_size, VFS_SEEK_SET) != 0)
            AUDERR ("seek error\n");
        else {
            imp3 = lame_get_lametag_frame(gfp, encbuffer, sizeof(encbuffer));
            if (file.fwrite (encbuffer, 1, imp3) != imp3)
                AUDERR ("write error\n");
        }
    }

    write_buffer.clear ();

    lame_close(gfp);
    AUDDBG("lame_close() done\n");
}

static int mp3_format_required (int fmt)
{
    return FMT_FLOAT;
}

FileWriterImpl mp3_plugin = {
    mp3_init,
    mp3_open,
    mp3_write,
    mp3_close,
    mp3_format_required,
};

#endif
