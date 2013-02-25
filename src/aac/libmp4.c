#include "config.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <neaacdec.h>

#include "mp4ff.h"
#include "tagging.h"

#include <audacious/plugin.h>
#include <audacious/i18n.h>

#define MP4_VERSION VERSION
#define SBR_DEC

/*
 * BUFFER_SIZE is the highest amount of memory that can be pulled.
 * We use this for sanity checks, among other things, as mp4ff needs
 * a labotomy sometimes.
 */
#define BUFFER_SIZE (FAAD_MIN_STREAMSIZE * 16)

static int mp4_is_our_fd (const char *, VFSFile *);

static const char *fmts[] = { "m4a", "mp4", "aac", NULL };

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int seek_value;
static bool_t stop_flag;

typedef struct _mp4cfg
{
#define FILE_OTHER      0
#define FILE_MP4        1
#define FILE_AAC        2
    short file_type;
} Mp4Config;

static Mp4Config mp4cfg;

void getMP4info (char *);
int getAACTrack (mp4ff_t *);

static uint32_t mp4_read_callback (void *data, void *buffer, uint32_t len)
{
    if (data == NULL || buffer == NULL)
        return -1;

    return vfs_fread (buffer, 1, len, (VFSFile *) data);
}

static uint32_t mp4_seek_callback (void *data, uint64_t pos)
{
    if (! data || pos > INT64_MAX)
        return -1;

    return vfs_fseek ((VFSFile *) data, pos, SEEK_SET);
}

static bool_t mp4_init (void)
{
    mp4cfg.file_type = FILE_OTHER;
    return TRUE;
}

static void mp4_stop (InputPlayback * p)
{
    pthread_mutex_lock (& mutex);

    if (! stop_flag)
    {
        stop_flag = TRUE;
        p->output->abort_write ();
    }

    pthread_mutex_unlock (& mutex);
}

static void mp4_pause (InputPlayback * p, bool_t pause)
{
    pthread_mutex_lock (& mutex);

    if (! stop_flag)
        p->output->pause (pause);

    pthread_mutex_unlock (& mutex);
}

static void mp4_seek (InputPlayback * p, int time)
{
    pthread_mutex_lock (& mutex);

    if (! stop_flag)
    {
        seek_value = time;
        p->output->abort_write();
    }

    pthread_mutex_unlock (& mutex);
}

/*
 * These routines are derived from MPlayer.
 */

/// \param srate (out) sample rate
/// \param num (out) number of audio frames in this ADTS frame
/// \return size of the ADTS frame in bytes
/// aac_parse_frames needs a buffer at least 8 bytes long
int aac_parse_frame (unsigned char * buf, int *srate, int *num)
{
    int i = 0, sr, fl = 0;
    static int srates[] =
     { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000,
         11025, 8000, 0, 0, 0 };

    if ((buf[i] != 0xFF) || ((buf[i + 1] & 0xF6) != 0xF0))
        return 0;

/* We currently have no use for the id below.
        id = (buf[i+1] >> 3) & 0x01;    //id=1 mpeg2, 0: mpeg4
*/
    sr = (buf[i + 2] >> 2) & 0x0F;
    if (sr > 11)
        return 0;
    *srate = srates[sr];

    fl =
     ((buf[i + 3] & 0x03) << 11) | (buf[i + 4] << 3) | ((buf[i +
     5] >> 5) & 0x07);
    *num = (buf[i + 6] & 0x02) + 1;

    return fl;
}

#define PROBE_DEBUG(...)

/* Searches <length> bytes of data for an ADTS header.  Returns the offset of
 * the first header or -1 if none is found.  Sets <size> to the length of the
 * frame. */
static int find_aac_header (unsigned char * data, int length, int * size)
{
    int offset, a, b;

    for (offset = 0; offset <= length - 8; offset++)
    {
        if (data[offset] != 255)
            continue;

        *size = aac_parse_frame (data + offset, &a, &b);

        if (*size < 8)
            continue;

        return offset;
    }

    return -1;
}

static bool_t parse_aac_stream (VFSFile * stream)
{
    unsigned char data[8192];
    int offset, found, inner, size;

    size = 0;                   /* avoid bogus uninitialized variable warning */

    if (vfs_fread (data, 1, sizeof data, stream) != sizeof data)
    {
        PROBE_DEBUG ("Read failed.\n");
        return FALSE;
    }

    offset = 0;

    for (found = 0; found < 3; found++)
    {
        inner = find_aac_header (data + offset, sizeof data - offset, &size);

        if (!(inner == 0 || (found == 0 && inner > 0)))
        {
            PROBE_DEBUG ("Only %d ADTS headers.\n", found);
            return FALSE;
        }

        offset += inner + size;
    }

    PROBE_DEBUG ("Accepted.\n");
    return TRUE;
}

/* Quick search for an ADTS or ADIF header in the first <len> bytes of <buf>.
 * Returns the byte offset of the header or <len> if none is found. */

static int aac_probe (unsigned char * buf, int len)
{
    for (int i = 0; i <= len - 4; i ++)
    {
        if ((buf[i] == 0xff && (buf[i + 1] & 0xf6) == 0xf0) || ! strncmp
         ((char *) buf + i, "ADIF", 4))
            return i;
    }

    return len;
}

static bool_t is_mp4_aac_file (VFSFile * handle)
{
    mp4ff_callback_t mp4_data = {.read = mp4_read_callback,.seek =
         mp4_seek_callback,.user_data = handle
    };
    mp4ff_t *mp4_handle = mp4ff_open_read (&mp4_data);
    bool_t success;

    if (mp4_handle == NULL)
        return FALSE;

    success = (getAACTrack (mp4_handle) != -1);

    mp4ff_close (mp4_handle);
    return success;
}

static bool_t mp4_is_our_fd (const char * filename, VFSFile * file)
{
    if (is_mp4_aac_file (file))
        return TRUE;

    if (vfs_fseek (file, 0, SEEK_SET))
        return FALSE;
    return parse_aac_stream (file);
}

/* Gets info (some approximated) from an AAC/ADTS file.  <length> is
 * milliseconds, <bitrate> is kilobits per second.  Any parameters that cannot
 * be read are set to -1. */
static void calc_aac_info (VFSFile * handle, int * length, int * bitrate,
 int * samplerate, int * channels)
{
    NeAACDecHandle decoder;
    NeAACDecFrameInfo frame;
    bool_t initted = FALSE;
    int size = vfs_fsize (handle);
    unsigned char buffer[BUFFER_SIZE];
    int offset = 0, filled = 0;
    int found, bytes_used = 0, time_used = 0;

    decoder = NULL;             /* avoid bogus uninitialized variable warning */

    *length = -1;
    *bitrate = -1;
    *samplerate = -1;
    *channels = -1;

    /* look for a representative bitrate in the middle of the file */
    if (size > 0 && vfs_fseek (handle, size / 2, SEEK_SET))
        goto DONE;

    for (found = 0; found < 32; found++)
    {
        if (filled < BUFFER_SIZE / 2)
        {
            memmove (buffer, buffer + offset, filled);
            offset = 0;

            if (vfs_fread (buffer + filled, 1, BUFFER_SIZE - filled, handle)
             != BUFFER_SIZE - filled)
            {
                PROBE_DEBUG ("Read failed.\n");
                goto DONE;
            }

            filled = BUFFER_SIZE;
        }

        if (!initted)
        {
            int inner, a;
            unsigned long r;
            unsigned char ch;

            inner = find_aac_header (buffer + offset, filled, &a);

            if (inner < 0)
            {
                PROBE_DEBUG ("No ADTS header.\n");
                goto DONE;
            }

            offset += inner;
            filled -= inner;

            decoder = NeAACDecOpen ();
            inner = NeAACDecInit (decoder, buffer + offset, filled, &r, &ch);

            if (inner < 0)
            {
                PROBE_DEBUG ("Decoder init failed.\n");
                NeAACDecClose (decoder);
                goto DONE;
            }

            offset += inner;
            filled -= inner;
            bytes_used += inner;

            *samplerate = r;
            *channels = ch;
            initted = TRUE;
        }

        if (NeAACDecDecode (decoder, &frame, buffer + offset, filled) == NULL)
        {
            PROBE_DEBUG ("Decode failed.\n");
            goto DONE;
        }

        if (frame.samplerate != *samplerate || frame.channels != *channels)
        {
            PROBE_DEBUG ("Parameter mismatch.\n");
            goto DONE;
        }

        offset += frame.bytesconsumed;
        filled -= frame.bytesconsumed;
        bytes_used += frame.bytesconsumed;
        time_used += frame.samples / frame.channels * (int64_t) 1000 /
         frame.samplerate;
    }

    /* bits per millisecond = kilobits per second */
    *bitrate = bytes_used * 8 / time_used;

    if (size > 0)
        *length = size * (int64_t) time_used / bytes_used;

  DONE:
    if (initted)
        NeAACDecClose (decoder);
}

static Tuple *aac_get_tuple (const char * filename, VFSFile * handle)
{
    Tuple *tuple = tuple_new_from_filename (filename);
    char *temp;
    int length, bitrate, samplerate, channels;

    tuple_set_str (tuple, FIELD_CODEC, NULL, "MPEG-2/4 AAC");

    if (!vfs_is_remote (filename))
    {
        calc_aac_info (handle, &length, &bitrate, &samplerate, &channels);

        if (length > 0)
            tuple_set_int (tuple, FIELD_LENGTH, NULL, length);

        if (bitrate > 0)
            tuple_set_int (tuple, FIELD_BITRATE, NULL, bitrate);
    }

    temp = vfs_get_metadata (handle, "track-name");
    if (temp != NULL)
    {
        tuple_set_str (tuple, FIELD_TITLE, NULL, temp);
        free (temp);
    }

    temp = vfs_get_metadata (handle, "stream-name");
    if (temp != NULL)
    {
        tuple_set_str (tuple, FIELD_ALBUM, NULL, temp);
        free (temp);
    }

    temp = vfs_get_metadata (handle, "content-bitrate");
    if (temp != NULL)
    {
        tuple_set_int (tuple, FIELD_BITRATE, NULL, atoi (temp) / 1000);
        free (temp);
    }

    return tuple;
}

static bool_t aac_title_changed (const char * filename, VFSFile * handle,
 Tuple * tuple)
{
    char *old = tuple_get_str (tuple, FIELD_TITLE, NULL);
    char *new = vfs_get_metadata (handle, "track-name");
    bool_t changed = FALSE;

    changed = (new != NULL && (old == NULL || strcmp (old, new)));
    if (changed)
        tuple_set_str (tuple, FIELD_TITLE, NULL, new);

    free (new);
    str_unref(old);
    return changed;
}

static void read_and_set_string (mp4ff_t * mp4, int (*func) (const mp4ff_t *
 mp4, char * *string), Tuple * tuple, int field)
{
    char *string = NULL;

    func (mp4, &string);

    if (string != NULL)
        tuple_set_str (tuple, field, NULL, string);

    free (string);
}

static Tuple *generate_tuple (const char * filename, mp4ff_t * mp4, int track)
{
    Tuple *tuple = tuple_new_from_filename (filename);
    int64_t length;
    int scale, rate, channels, bitrate;
    char *year = NULL, *cd_track = NULL;
    char scratch[32];

    tuple_set_str (tuple, FIELD_CODEC, NULL, "MPEG-2/4 AAC");

    length = mp4ff_get_track_duration (mp4, track);
    scale = mp4ff_time_scale (mp4, track);

    if (length > 0 && scale > 0)
        tuple_set_int (tuple, FIELD_LENGTH, NULL, length * 1000 / scale);

    rate = mp4ff_get_sample_rate (mp4, track);
    channels = mp4ff_get_channel_count (mp4, track);

    if (rate > 0 && channels > 0)
    {
        snprintf (scratch, sizeof scratch, "%d kHz, %s", rate / 1000, channels
         == 1 ? _("mono") : channels == 2 ? _("stereo") : _("surround"));
        tuple_set_str (tuple, FIELD_QUALITY, NULL, scratch);
    }

    bitrate = mp4ff_get_avg_bitrate (mp4, track);

    if (bitrate > 0)
        tuple_set_int (tuple, FIELD_BITRATE, NULL, bitrate / 1000);

    read_and_set_string (mp4, mp4ff_meta_get_title, tuple, FIELD_TITLE);
    read_and_set_string (mp4, mp4ff_meta_get_album, tuple, FIELD_ALBUM);
    read_and_set_string (mp4, mp4ff_meta_get_artist, tuple, FIELD_ARTIST);
    read_and_set_string (mp4, mp4ff_meta_get_comment, tuple, FIELD_COMMENT);
    read_and_set_string (mp4, mp4ff_meta_get_genre, tuple, FIELD_GENRE);

    mp4ff_meta_get_date (mp4, &year);

    if (year != NULL)
        tuple_set_int (tuple, FIELD_YEAR, NULL, atoi (year));

    free (year);

    mp4ff_meta_get_track (mp4, &cd_track);

    if (cd_track != NULL)
        tuple_set_int (tuple, FIELD_TRACK_NUMBER, NULL, atoi (cd_track));

    free (cd_track);

    return tuple;
}

static Tuple *mp4_get_tuple (const char * filename, VFSFile * handle)
{
    mp4ff_callback_t mp4cb;
    mp4ff_t *mp4;
    int track;
    Tuple *tuple;

    if (parse_aac_stream (handle))
        return aac_get_tuple (filename, handle);

    vfs_rewind (handle);

    mp4cb.read = mp4_read_callback;
    mp4cb.seek = mp4_seek_callback;
    mp4cb.user_data = handle;

    mp4 = mp4ff_open_read (&mp4cb);

    if (mp4 == NULL)
        return NULL;

    track = getAACTrack (mp4);

    if (track < 0)
    {
        mp4ff_close (mp4);
        return NULL;
    }

    tuple = generate_tuple (filename, mp4, track);
    mp4ff_close (mp4);
    return tuple;
}

static bool_t my_decode_mp4 (InputPlayback * playback, const char * filename,
 mp4ff_t * mp4file, bool_t pause)
{
    // We are reading an MP4 file
    int mp4track = getAACTrack (mp4file);
    NeAACDecHandle decoder;
    NeAACDecConfigurationPtr decoder_config;
    unsigned char *buffer = NULL;
    unsigned bufferSize = 0;
    unsigned long samplerate = 0;
    unsigned char channels = 0;
    unsigned numSamples;
    unsigned long sampleID = 1;
    unsigned framesize = 0;

    if (mp4track < 0)
    {
        fprintf (stderr, "Unsupported Audio track type\n");
        return TRUE;
    }

    // Open decoder
    decoder = NeAACDecOpen ();

    // Configure for floating point output
    decoder_config = NeAACDecGetCurrentConfiguration (decoder);
    decoder_config->outputFormat = FAAD_FMT_FLOAT;
    NeAACDecSetConfiguration (decoder, decoder_config);

    mp4ff_get_decoder_config (mp4file, mp4track, &buffer, &bufferSize);
    if (!buffer)
    {
        NeAACDecClose (decoder);
        return FALSE;
    }
    if (NeAACDecInit2 (decoder, buffer, bufferSize, &samplerate, &channels) < 0)
    {
        NeAACDecClose (decoder);

        return FALSE;
    }

    free (buffer);
    if (!channels)
    {
        NeAACDecClose (decoder);

        return FALSE;
    }
    numSamples = mp4ff_num_samples (mp4file, mp4track);

    if (!playback->output->open_audio (FMT_FLOAT, samplerate, channels))
    {
        NeAACDecClose (decoder);
        return FALSE;
    }

    playback->output->pause (pause);
    playback->set_tuple (playback, generate_tuple (filename, mp4file, mp4track));
    playback->set_params (playback, mp4ff_get_avg_bitrate (mp4file, mp4track),
     samplerate, channels);
    playback->set_pb_ready (playback);

    while (1)
    {
        void *sampleBuffer;
        NeAACDecFrameInfo frameInfo;
        int rc;

        buffer = NULL;
        bufferSize = 0;

        /* If we've run to the end of the file, we're done. */
        if (sampleID >= numSamples)
            break;

        rc = mp4ff_read_sample (mp4file, mp4track,
         sampleID++, &buffer, &bufferSize);

        /* If we can't read the file, we're done. */
        if ((rc == 0) || (buffer == NULL) || (bufferSize == 0)
         || (bufferSize > BUFFER_SIZE))
        {
            fprintf (stderr, "MP4: read error\n");
            sampleBuffer = NULL;

            NeAACDecClose (decoder);

            return FALSE;
        }

        sampleBuffer = NeAACDecDecode (decoder, &frameInfo, buffer, bufferSize);

        /* If there was an error decoding, we're done. */
        if (frameInfo.error > 0)
        {
            fprintf (stderr, "MP4: %s\n", NeAACDecGetErrorMessage (frameInfo.error));
            NeAACDecClose (decoder);

            return FALSE;
        }
        if (buffer)
        {
            free (buffer);
            buffer = NULL;
            bufferSize = 0;
        }

        /* Calculate frame size from the first (non-blank) frame.  This needs to
         * be done before we try to seek. */
        if (!framesize)
        {
            framesize = frameInfo.samples / frameInfo.channels;

            if (!framesize)
                continue;
        }

        /* Respond to seek/stop requests.  This needs to be done after we
         * calculate frame size but of course before we write any audio. */
        pthread_mutex_lock (& mutex);

        if (stop_flag)
        {
            pthread_mutex_unlock (& mutex);
            break;
        }

        if (seek_value >= 0)
        {
            sampleID = (int64_t) seek_value * samplerate / 1000 / framesize;
            playback->output->flush (seek_value);
            seek_value = -1;
        }

        pthread_mutex_unlock (& mutex);

        playback->output->write_audio (sampleBuffer, sizeof (float) * frameInfo.samples);
    }

    pthread_mutex_lock (& mutex);
    stop_flag = TRUE;
    pthread_mutex_unlock (& mutex);

    NeAACDecClose (decoder);

    return TRUE;
}

static void aac_seek (VFSFile * file, NeAACDecHandle dec, int time, int len,
 void * buf, int size, int * buflen)
{
    /* == ESTIMATE BYTE OFFSET == */

    int64_t total = vfs_fsize (file);
    if (total < 0)
    {
        fprintf (stderr, "aac: File is not seekable.\n");
        return;
    }

    /* == SEEK == */

    if (vfs_fseek (file, total * time / len, SEEK_SET))
        return;

    * buflen = vfs_fread (buf, 1, size, file);

    /* == FIND FRAME HEADER == */

    int used = aac_probe (buf, * buflen);

    if (used == * buflen)
    {
        fprintf (stderr, "aac: No valid frame header found.\n");
        * buflen = 0;
        return;
    }

    if (used)
    {
        * buflen -= used;
        memmove (buf, (char *) buf + used, * buflen);
        * buflen += vfs_fread ((char *) buf + * buflen, 1, size - * buflen, file);
    }

    /* == START DECODING == */

    unsigned char chan;
    unsigned long rate;

    if ((used = NeAACDecInit (dec, buf, * buflen, & rate, & chan)))
    {
        * buflen -= used;
        memmove (buf, (char *) buf + used, * buflen);
        * buflen += vfs_fread ((char *) buf + * buflen, 1, size - * buflen, file);
    }
}

static bool_t my_decode_aac (InputPlayback * playback, const char * filename,
 VFSFile * file, bool_t pause)
{
    NeAACDecHandle decoder = 0;
    NeAACDecConfigurationPtr decoder_config;
    unsigned long samplerate = 0;
    unsigned char channels = 0;
    Tuple *tuple;
    int bitrate = 0;

    tuple = aac_get_tuple (filename, file);
    if (tuple != NULL)
    {
        tuple_ref (tuple);
        playback->set_tuple (playback, tuple);

        bitrate = tuple_get_int (tuple, FIELD_BITRATE, NULL);
        bitrate = 1000 * MAX (0, bitrate);
    }

    vfs_rewind (file);
    if ((decoder = NeAACDecOpen ()) == NULL)
    {
        fprintf (stderr, "AAC: Open Decoder Error\n");
        return FALSE;
    }

    decoder_config = NeAACDecGetCurrentConfiguration (decoder);
    decoder_config->outputFormat = FAAD_FMT_FLOAT;
    NeAACDecSetConfiguration (decoder, decoder_config);

    /* == FILL BUFFER == */

    unsigned char buf[BUFFER_SIZE];
    int buflen = vfs_fread (buf, 1, sizeof buf, file);

    /* == SKIP ID3 TAG == */

    if (buflen >= 10 && ! strncmp ((char *) buf, "ID3", 3))
    {
        if (vfs_fseek (file, 10 + (buf[6] << 21) + (buf[7] << 14) + (buf[8] <<
         7) + buf[9], SEEK_SET))
        {
            fprintf (stderr, "aac: Failed to seek past ID3v2 tag.\n");
            goto ERR_CLOSE_DECODER;
        }

        buflen = vfs_fread (buf, 1, sizeof buf, file);
    }

    /* == FIND FRAME HEADER == */

    int used = aac_probe (buf, buflen);

    if (used == buflen)
    {
        fprintf (stderr, "aac: No valid frame header found.\n");
        goto ERR_CLOSE_DECODER;
    }

    if (used)
    {
        buflen -= used;
        memmove (buf, buf + used, buflen);
        buflen += vfs_fread (buf + buflen, 1, sizeof buf - buflen, file);
    }

    /* == START DECODING == */

    if ((used = NeAACDecInit (decoder, buf, buflen, & samplerate, & channels)))
    {
        buflen -= used;
        memmove (buf, buf + used, buflen);
        buflen += vfs_fread (buf + buflen, 1, sizeof buf - buflen, file);
    }

    /* == CHECK FOR METADATA == */

    if (tuple && aac_title_changed (filename, file, tuple))
    {
        tuple_ref (tuple);
        playback->set_tuple (playback, tuple);
    }

    /* == START PLAYBACK == */

    if (! playback->output->open_audio (FMT_FLOAT, samplerate, channels))
        goto ERR_CLOSE_DECODER;

    playback->output->pause (pause);
    playback->set_params (playback, bitrate, samplerate, channels);
    playback->set_pb_ready (playback);

    /* == MAIN LOOP == */

    while (1)
    {
        pthread_mutex_lock (& mutex);

        /* == HANDLE STOP REQUESTS == */

        if (stop_flag)
        {
            pthread_mutex_unlock (& mutex);
            break;
        }

        /* == HANDLE SEEK REQUESTS == */

        if (seek_value >= 0)
        {
            int length = tuple ? tuple_get_int (tuple, FIELD_LENGTH, NULL) : 0;

            if (length > 0)
            {
                aac_seek (file, decoder, seek_value, length, buf, sizeof buf, & buflen);
                playback->output->flush (seek_value);
            }

            seek_value = -1;
        }

        pthread_mutex_unlock (& mutex);

        /* == CHECK FOR END OF FILE == */

        if (! buflen)
            break;

        /* == CHECK FOR METADATA == */

        if (tuple && aac_title_changed (filename, file, tuple))
        {
            tuple_ref (tuple);
            playback->set_tuple (playback, tuple);
        }

        /* == DECODE A FRAME == */

        NeAACDecFrameInfo info;
        void * audio = NeAACDecDecode (decoder, & info, buf, buflen);

        if (info.error)
        {
            fprintf (stderr, "aac: %s.\n", NeAACDecGetErrorMessage (info.error));

            if (buflen)
            {
                used = 1 + aac_probe (buf + 1, buflen - 1);
                buflen -= used;
                memmove (buf, buf + used, buflen);
                buflen += vfs_fread (buf + buflen, 1, sizeof buf - buflen, file);
            }

            continue;
        }

        if ((used = info.bytesconsumed))
        {
            buflen -= used;
            memmove (buf, buf + used, buflen);
            buflen += vfs_fread (buf + buflen, 1, sizeof buf - buflen, file);
        }

        /* == PLAY THE SOUND == */

        if (audio && info.samples)
            playback->output->write_audio (audio, sizeof (float) * info.samples);
    }

    pthread_mutex_lock (& mutex);
    stop_flag = TRUE;
    pthread_mutex_unlock (& mutex);

    NeAACDecClose (decoder);

    if (tuple)
        tuple_unref (tuple);

    return TRUE;

ERR_CLOSE_DECODER:
    NeAACDecClose (decoder);

    if (tuple)
        tuple_unref (tuple);

    return FALSE;
}

static bool_t mp4_play (InputPlayback * playback, const char * filename,
 VFSFile * file, int start_time, int stop_time, bool_t pause)
{
    if (! file)
        return FALSE;

    mp4ff_callback_t mp4cb;
    mp4ff_t *mp4file;
    bool_t ret;

    ret = parse_aac_stream (file);
    vfs_rewind (file);

    memset (& mp4cb, 0, sizeof (mp4ff_callback_t));
    mp4cb.read = mp4_read_callback;
    mp4cb.seek = mp4_seek_callback;
    mp4cb.user_data = file;

    mp4file = mp4ff_open_read (& mp4cb);

    seek_value = (start_time > 0) ? start_time : -1;
    stop_flag = FALSE;

    if (ret == TRUE)
    {
        return my_decode_aac (playback, filename, file, pause);
    }

    return my_decode_mp4 (playback, filename, mp4file, pause);
}

bool_t read_itunes_cover (const char * filename, VFSFile * file, void * *
 data, int64_t * size);

AUD_INPUT_PLUGIN
(
    .name = N_("AAC Decoder"),
    .domain = PACKAGE,
    .init = mp4_init,
    .play = mp4_play,
    .stop = mp4_stop,
    .pause = mp4_pause,
    .mseek = mp4_seek,
    .is_our_file_from_vfs = mp4_is_our_fd,
    .probe_for_tuple = mp4_get_tuple,
    .get_song_image = read_itunes_cover,
    .extensions = fmts,
)
