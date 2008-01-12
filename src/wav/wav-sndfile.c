/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005 Audacious development team.
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
 */

#include "config.h"

#include <glib.h>
#include <string.h>
#include <math.h>

#include <audacious/plugin.h>
#include <audacious/util.h>
#include <audacious/i18n.h>
#include <audacious/main.h>
#include <audacious/output.h>
#include "wav-sndfile.h"

#include <sndfile.h>

static SNDFILE *sndfile = NULL;
static SF_INFO sfinfo;

static int song_length;
static int bit_rate = 0;
static glong seek_time = -1;

static GThread *decode_thread;
static GMutex *decode_mutex;
static GCond *decode_cond;




static sf_count_t sf_get_filelen (void *user_data)
{
    return aud_vfs_fsize (user_data);
}
static sf_count_t sf_vseek (sf_count_t offset, int whence, void *user_data)
{
    return aud_vfs_fseek(user_data, offset, whence);
}
static sf_count_t sf_vread (void *ptr, sf_count_t count, void *user_data)
{
    return aud_vfs_fread(ptr, 1, count, user_data);
}
static sf_count_t sf_vwrite (const void *ptr, sf_count_t count, void *user_data)
{
    return aud_vfs_fwrite(ptr, 1, count, user_data);
}
static sf_count_t sf_tell (void *user_data)
{
    return aud_vfs_ftell(user_data);
}
static SF_VIRTUAL_IO sf_virtual_io =
{
    sf_get_filelen,
    sf_vseek,
    sf_vread,
    sf_vwrite,
    sf_tell
};


static void
plugin_init (void)
{
    seek_time = -1;

    decode_mutex = g_mutex_new();
    decode_cond = g_cond_new();
}

static void
plugin_cleanup (void)
{
    g_cond_free(decode_cond);
    g_mutex_free(decode_mutex);
}

static int
get_song_length (char *filename)
{
    SNDFILE *tmp_sndfile;
    SF_INFO tmp_sfinfo;
    gchar *realfn = NULL;

    realfn = g_filename_from_uri(filename, NULL, NULL);
    tmp_sndfile = sf_open (realfn ? realfn : filename, SFM_READ, &tmp_sfinfo);
    g_free(realfn); realfn = NULL;

    if (!tmp_sndfile) {
        return 0;
    }

    sf_close (tmp_sndfile);
    tmp_sndfile = NULL;

    if (tmp_sfinfo.samplerate <= 0)
        return 0;

    return (int) ceil (1000.0 * tmp_sfinfo.frames / tmp_sfinfo.samplerate);
}

static void
fill_song_tuple (char *filename, Tuple *ti)
{
    SNDFILE *tmp_sndfile;
    SF_INFO tmp_sfinfo;
    unsigned int lossy = 0;
    gchar *realfn = NULL, *codec = NULL, *format, *subformat = NULL;
    GString *codec_gs = NULL;

    realfn = g_filename_from_uri(filename, NULL, NULL);
    tmp_sndfile = sf_open (realfn ? realfn : filename, SFM_READ, &tmp_sfinfo);
    if ( sf_get_string(tmp_sndfile, SF_STR_TITLE) == NULL)
        aud_tuple_associate_string(ti, FIELD_TITLE, NULL, g_path_get_basename(realfn ? realfn : filename));
    else
        aud_tuple_associate_string(ti, FIELD_TITLE, NULL, sf_get_string(tmp_sndfile, SF_STR_TITLE));

    aud_tuple_associate_string(ti, FIELD_ARTIST, NULL, sf_get_string(tmp_sndfile, SF_STR_ARTIST));
    aud_tuple_associate_string(ti, FIELD_COMMENT, NULL, sf_get_string(tmp_sndfile, SF_STR_COMMENT));
    aud_tuple_associate_string(ti, FIELD_DATE, NULL, sf_get_string(tmp_sndfile, SF_STR_DATE));
    aud_tuple_associate_string(ti, -1, "software", sf_get_string(tmp_sndfile, SF_STR_SOFTWARE));

    g_free(realfn); realfn = NULL;

    if (!tmp_sndfile)
        return;

    sf_close (tmp_sndfile);
    tmp_sndfile = NULL;

    if (tmp_sfinfo.samplerate > 0)
        aud_tuple_associate_int(ti, FIELD_LENGTH, NULL, (int) ceil (1000.0 * tmp_sfinfo.frames / tmp_sfinfo.samplerate));

    switch (tmp_sfinfo.format & SF_FORMAT_TYPEMASK)
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
            format = "unknown sndfile";
    }
    switch (tmp_sfinfo.format & SF_FORMAT_SUBMASK)
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
            lossy = 1;
            break;
        case SF_FORMAT_ALAW:
            subformat = "A-Law";
            lossy = 1;
            break;
        case SF_FORMAT_IMA_ADPCM:
            subformat = "IMA ADPCM";
            lossy = 1;
            break;
        case SF_FORMAT_MS_ADPCM:
            subformat = "MS ADPCM";
            lossy = 1;
            break;
        case SF_FORMAT_GSM610:
            subformat = "GSM 6.10";
            lossy = 1;
            break;
        case SF_FORMAT_VOX_ADPCM:
            subformat = "Oki Dialogic ADPCM";
            lossy = 1;
            break;
        case SF_FORMAT_G721_32:
            subformat = "32kbs G721 ADPCM";
            lossy = 1;
            break;
        case SF_FORMAT_G723_24:
            subformat = "24kbs G723 ADPCM";
            lossy = 1;
            break;
        case SF_FORMAT_G723_40:
            subformat = "40kbs G723 ADPCM";
            lossy = 1;
            break;
        case SF_FORMAT_DWVW_12:
            subformat = "12 bit Delta Width Variable Word";
            lossy = 1;
            break;
        case SF_FORMAT_DWVW_16:
            subformat = "16 bit Delta Width Variable Word";
            lossy = 1;
            break;
        case SF_FORMAT_DWVW_24:
            subformat = "24 bit Delta Width Variable Word";
            lossy = 1;
            break;
        case SF_FORMAT_DWVW_N:
            subformat = "N bit Delta Width Variable Word";
            lossy = 1;
            break;
        case SF_FORMAT_DPCM_8:
            subformat = "8 bit differential PCM";
            break;
        case SF_FORMAT_DPCM_16:
            subformat = "16 bit differential PCM";
    }

    codec_gs = g_string_new("");
    if (subformat != NULL)
        g_string_append_printf(codec_gs, "%s (%s)", format, subformat);
    else
        g_string_append_printf(codec_gs, "%s", format);
    codec = g_strdup(codec_gs->str);
    g_string_free(codec_gs, TRUE);
    aud_tuple_associate_string(ti, FIELD_CODEC, NULL, codec);

    if (lossy != 0)
        aud_tuple_associate_string(ti, FIELD_QUALITY, NULL, "lossy");
    else
        aud_tuple_associate_string(ti, FIELD_QUALITY, NULL, "lossless");
}

static gchar *get_title(char *filename)
{
    Tuple *tuple;
    gchar *title;

    tuple = aud_tuple_new_from_filename(filename);
    fill_song_tuple(filename, tuple);
    title = aud_tuple_formatter_make_title_string(tuple, aud_get_gentitle_format());
    if (*title == '\0')
    {
        g_free(title);
        title = g_strdup(aud_tuple_get_string(tuple, FIELD_FILE_NAME, NULL));
    }

    aud_tuple_free(tuple);
    return title;
}

static int
is_our_file (char *filename)
{
    SNDFILE *tmp_sndfile;
    SF_INFO tmp_sfinfo;
    gchar *realfn = NULL; 

    realfn = g_filename_from_uri(filename, NULL, NULL);

    /* Have to open the file to see if libsndfile can handle it. */
    tmp_sndfile = sf_open (realfn ? realfn : filename, SFM_READ, &tmp_sfinfo);
    g_free(realfn); realfn = NULL;

    if (!tmp_sndfile) {
        return FALSE;
    }

    /* It can so close file and return TRUE. */
    sf_close (tmp_sndfile);
    tmp_sndfile = NULL;

    return TRUE;
}

static gpointer
play_loop (gpointer arg)
{
    static short buffer [BUFFER_SIZE];
    int samples;
    InputPlayback *playback = arg;

    for (;;)
    {
        GTimeVal sleeptime;

        /* sf_read_short will return 0 for all reads at EOF. */
        samples = sf_read_short (sndfile, buffer, BUFFER_SIZE);

        if (samples > 0 && playback->playing == TRUE) {
            while ((playback->output->buffer_free () < samples) &&
                   playback->playing == TRUE) {
                g_get_current_time(&sleeptime);
                g_time_val_add(&sleeptime, 500000);
                g_mutex_lock(decode_mutex);
                g_cond_timed_wait(decode_cond, decode_mutex, &sleeptime);
                g_mutex_unlock(decode_mutex);

                if (playback->playing == FALSE)
                    break;
            }

            playback->pass_audio(playback, FMT_S16_NE, sfinfo.channels, 
                                 samples * sizeof (short), buffer, &playback->playing);
        }
        else {
            while(playback->output->buffer_playing()) {
                g_get_current_time(&sleeptime);
                g_time_val_add(&sleeptime, 500000);
                g_mutex_lock(decode_mutex);
                g_cond_timed_wait(decode_cond, decode_mutex, &sleeptime);
                g_mutex_unlock(decode_mutex);

                if(playback->playing == FALSE)
                    break;
            }

            playback->eof = TRUE;
            playback->playing = FALSE;

            g_mutex_unlock(decode_mutex);
            break;
        }

        /* Do seek if seek_time is valid. */
        if (seek_time >= 0) {
            sf_seek (sndfile, (sf_count_t)((gint64)seek_time * (gint64)sfinfo.samplerate / 1000L),
                     SEEK_SET);
            playback->output->flush (seek_time);
            seek_time = -1;
        }

        if (playback->playing == FALSE)
            break;
    }

    sf_close (sndfile);
    sndfile = NULL;
    seek_time = -1;

    playback->output->close_audio();

    return NULL;
}

static void
play_start (InputPlayback *playback)
{
    gchar *realfn = NULL;
    int pcmbitwidth;
    gchar *song_title;

    if (sndfile) /* already opened */
        return;

    pcmbitwidth = 32;
    song_title = get_title(playback->filename);

    realfn = g_filename_from_uri(playback->filename, NULL, NULL);
    sndfile = sf_open (realfn ? realfn : playback->filename, SFM_READ, &sfinfo);
    g_free(realfn); realfn = NULL;

    if (!sndfile)
        return;

    bit_rate = sfinfo.samplerate * pcmbitwidth;

    if (sfinfo.samplerate > 0)
        song_length = (int) ceil (1000.0 * sfinfo.frames / sfinfo.samplerate);
    else
        song_length = 0;

    if (! playback->output->open_audio (FMT_S16_NE, sfinfo.samplerate, sfinfo.channels))
    {
        sf_close (sndfile);
        sndfile = NULL;
        return;
    }

    playback->set_params(playback, song_title, song_length, bit_rate, sfinfo.samplerate, sfinfo.channels);
    g_free (song_title);

    playback->playing = TRUE;

    decode_thread = g_thread_self();
    playback->set_pb_ready(playback);
    play_loop(playback);
}

static void
play_pause (InputPlayback *playback, gshort p)
{
    playback->output->pause(p);
}

static void
play_stop (InputPlayback *playback)
{
    if (decode_thread == NULL)
        return;

    g_mutex_lock(decode_mutex);
    playback->playing = FALSE;
    g_mutex_unlock(decode_mutex);
    g_cond_signal(decode_cond);

    g_thread_join (decode_thread);

    sndfile = NULL;
    decode_thread = NULL;
    seek_time = -1;
}

static void
file_mseek (InputPlayback *playback, gulong millisecond)
{
    if (! sfinfo.seekable)
        return;

    seek_time = (glong)millisecond;

    while (seek_time != -1)
        g_usleep (80000);
}

static void
file_seek (InputPlayback *playback, gint time)
{
    gulong millisecond = time * 1000;
    file_mseek(playback, millisecond);
}

static void
get_song_info (gchar *filename, gchar **title, gint *length)
{
    (*length) = get_song_length(filename);
    (*title) = get_title(filename);
}

static Tuple*
get_song_tuple (gchar *filename)
{
    Tuple *ti = aud_tuple_new_from_filename(filename);
    fill_song_tuple(filename, ti);
    return ti;
}

static int is_our_file_from_vfs(char *filename, VFSFile *fin)
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

static void wav_about(void)
{
    static GtkWidget *box;
    if (!box)
    {
        box = audacious_info_dialog(_("About sndfile WAV support"),
                                    _("Adapted for Audacious usage by Tony Vroon <chainsaw@gentoo.org>\n"
                                      "from the xmms_sndfile plugin which is:\n"
                                      "Copyright (C) 2000, 2002 Erik de Castro Lopo\n\n"
                                      "This program is free software ; you can redistribute it and/or modify \n"
                                      "it under the terms of the GNU General Public License as published by \n"
                                      "the Free Software Foundation ; either version 2 of the License, or \n"
                                      "(at your option) any later version. \n \n"
                                      "This program is distributed in the hope that it will be useful, \n"
                                      "but WITHOUT ANY WARRANTY ; without even the implied warranty of \n"
                                      "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  \n"
                                      "See the GNU General Public License for more details. \n\n"
                                      "You should have received a copy of the GNU General Public \n"
                                      "License along with this program ; if not, write to \n"
                                      "the Free Software Foundation, Inc., \n"
                                      "51 Franklin Street, Fifth Floor, \n"
                                      "Boston, MA  02110-1301  USA"),
                                    _("Ok"), FALSE, NULL, NULL);
        g_signal_connect(G_OBJECT(box), "destroy",
                         (GCallback)gtk_widget_destroyed, &box);
    }
}

static gchar *fmts[] = { "aiff", "au", "raw", "wav", NULL };

InputPlugin wav_ip = {
    .description = "sndfile WAV plugin",
    .init = plugin_init,
    .about = wav_about,
    .is_our_file = is_our_file,
    .play_file = play_start,
    .stop = play_stop,
    .pause = play_pause,
    .seek = file_seek,
    .cleanup = plugin_cleanup,
    .get_song_info = get_song_info,
    .get_song_tuple = get_song_tuple,
    //.is_our_file_from_vfs = is_our_file_from_vfs,
    .vfs_extensions = fmts,
    .mseek = file_mseek,
};

InputPlugin *wav_iplist[] = { &wav_ip, NULL };

SIMPLE_INPUT_PLUGIN(wav-sndfile, wav_iplist)
