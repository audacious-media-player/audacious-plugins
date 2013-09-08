#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <neaacdec.h>

#include "mp4ff.h"
#include "tagging.h"

#include <audacious/plugin.h>
#include <audacious/i18n.h>

/*
 * BUFFER_SIZE is the highest amount of memory that can be pulled.
 * We use this for sanity checks, among other things, as mp4ff needs
 * a labotomy sometimes.
 */
#define BUFFER_SIZE (FAAD_MIN_STREAMSIZE * 16)

static const char *fmts[] = { "m4a", "mp4", NULL };

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

static bool_t is_mp4_aac_file (const char * filename, VFSFile * handle)
{
    mp4ff_callback_t mp4_data = {
        .read = mp4_read_callback,
        .seek = mp4_seek_callback,
        .user_data = handle
    };

    mp4ff_t *mp4_handle = mp4ff_open_read (&mp4_data);
    bool_t success;

    if (mp4_handle == NULL)
        return FALSE;

    success = (getAACTrack (mp4_handle) != -1);

    mp4ff_close (mp4_handle);
    return success;
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
 mp4ff_t * mp4file)
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

    playback->set_tuple (playback, generate_tuple (filename, mp4file, mp4track));
    playback->set_params (playback, mp4ff_get_avg_bitrate (mp4file, mp4track),
     samplerate, channels);

    while (! playback->check_stop ())
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
        int seek_value = playback->check_seek ();

        if (seek_value >= 0)
        {
            sampleID = (int64_t) seek_value * samplerate / 1000 / framesize;
            continue;
        }

        playback->output->write_audio (sampleBuffer, sizeof (float) * frameInfo.samples);
    }

    NeAACDecClose (decoder);

    return TRUE;
}

static bool_t mp4_play (InputPlayback * playback, const char * filename,
 VFSFile * file)
{
    bool_t result;

    mp4ff_callback_t mp4cb = {
        .read = mp4_read_callback,
        .seek = mp4_seek_callback,
        .user_data = file
    };

    mp4ff_t * mp4file = mp4ff_open_read (& mp4cb);
    result = my_decode_mp4 (playback, filename, mp4file);
    mp4ff_close (mp4file);

    return result;
}

bool_t read_itunes_cover (const char * filename, VFSFile * file, void * *
 data, int64_t * size);

AUD_INPUT_PLUGIN
(
    .name = N_("AAC (MP4) Decoder"),
    .domain = PACKAGE,
    .play = mp4_play,
    .is_our_file_from_vfs = is_mp4_aac_file,
    .probe_for_tuple = mp4_get_tuple,
    .get_song_image = read_itunes_cover,
    .extensions = fmts,
)
