#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <neaacdec.h>

#include <audacious/audtag.h>
#include <libaudcore/input.h>
#include <libaudcore/plugin.h>
#include <libaudcore/i18n.h>

/*
 * BUFFER_SIZE is the highest amount of memory that can be pulled.
 * We use this for sanity checks, among other things, as mp4ff needs
 * a labotomy sometimes.
 */
#define BUFFER_SIZE (FAAD_MIN_STREAMSIZE * 16)

static const char *fmts[] = { "aac", nullptr };

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

static bool parse_aac_stream (const char * filename, VFSFile * stream)
{
    unsigned char data[8192];
    int offset, found, inner, size;

    size = 0;                   /* avoid bogus uninitialized variable warning */

    if (vfs_fread (data, 1, sizeof data, stream) != sizeof data)
    {
        PROBE_DEBUG ("Read failed.\n");
        return false;
    }

    offset = 0;

    for (found = 0; found < 3; found++)
    {
        inner = find_aac_header (data + offset, sizeof data - offset, &size);

        if (!(inner == 0 || (found == 0 && inner > 0)))
        {
            PROBE_DEBUG ("Only %d ADTS headers.\n", found);
            return false;
        }

        offset += inner + size;
    }

    PROBE_DEBUG ("Accepted.\n");
    return true;
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

/* Gets info (some approximated) from an AAC/ADTS file.  <length> is
 * milliseconds, <bitrate> is kilobits per second.  Any parameters that cannot
 * be read are set to -1. */
static void calc_aac_info (VFSFile * handle, int * length, int * bitrate,
 int * samplerate, int * channels)
{
    NeAACDecHandle decoder;
    NeAACDecFrameInfo frame;
    bool initted = false;
    int size = vfs_fsize (handle);
    unsigned char buffer[BUFFER_SIZE];
    int offset = 0, filled = 0;
    int found, bytes_used = 0, time_used = 0;

    decoder = nullptr;             /* avoid bogus uninitialized variable warning */

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
            initted = true;
        }

        if (NeAACDecDecode (decoder, &frame, buffer + offset, filled) == nullptr)
        {
            PROBE_DEBUG ("Decode failed.\n");
            goto DONE;
        }

        if ((int)frame.samplerate != *samplerate || (int)frame.channels != *channels)
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

static Tuple aac_get_tuple (const char * filename, VFSFile * handle)
{
    Tuple tuple;
    int length, bitrate, samplerate, channels;

    tuple.set_filename (filename);
    tuple.set_str (FIELD_CODEC, "MPEG-2/4 AAC");

    if (!vfs_is_remote (filename))
    {
        calc_aac_info (handle, &length, &bitrate, &samplerate, &channels);

        if (length > 0)
            tuple.set_int (FIELD_LENGTH, length);

        if (bitrate > 0)
            tuple.set_int (FIELD_BITRATE, bitrate);
    }

    tag_update_stream_metadata (tuple, handle);

    return tuple;
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

    int used = aac_probe ((unsigned char *) buf, * buflen);

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

    if ((used = NeAACDecInit (dec, (unsigned char *) buf, * buflen, & rate, & chan)))
    {
        * buflen -= used;
        memmove (buf, (char *) buf + used, * buflen);
        * buflen += vfs_fread ((char *) buf + * buflen, 1, size - * buflen, file);
    }
}

static bool my_decode_aac (const char * filename, VFSFile * file)
{
    NeAACDecHandle decoder = 0;
    NeAACDecConfigurationPtr decoder_config;
    unsigned long samplerate = 0;
    unsigned char channels = 0;
    int bitrate = 0;

    Tuple tuple = aud_input_get_tuple ();

    if (tuple)
    {
        bitrate = tuple.get_int (FIELD_BITRATE);
        bitrate = 1000 * aud::max (0, bitrate);
    }

    if ((decoder = NeAACDecOpen ()) == nullptr)
    {
        fprintf (stderr, "AAC: Open Decoder Error\n");
        return false;
    }

    decoder_config = NeAACDecGetCurrentConfiguration (decoder);
    decoder_config->outputFormat = FAAD_FMT_FLOAT;
    NeAACDecSetConfiguration (decoder, decoder_config);

    /* == FILL BUFFER == */

    unsigned char buf[BUFFER_SIZE];
    int buflen;
    buflen = vfs_fread (buf, 1, sizeof buf, file);

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

    int used;
    used = aac_probe (buf, buflen);

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

    if (tuple && tag_update_stream_metadata (tuple, file))
        aud_input_set_tuple (tuple.ref ());

    /* == START PLAYBACK == */

    if (! aud_input_open_audio (FMT_FLOAT, samplerate, channels))
        goto ERR_CLOSE_DECODER;

    aud_input_set_bitrate (bitrate);

    /* == MAIN LOOP == */

    while (! aud_input_check_stop ())
    {
        /* == HANDLE SEEK REQUESTS == */

        int seek_value = aud_input_check_seek ();

        if (seek_value >= 0)
        {
            int length = tuple ? tuple.get_int (FIELD_LENGTH) : 0;

            if (length > 0)
                aac_seek (file, decoder, seek_value, length, buf, sizeof buf, & buflen);
        }

        /* == CHECK FOR END OF FILE == */

        if (! buflen)
            break;

        /* == CHECK FOR METADATA == */

        if (tuple && tag_update_stream_metadata (tuple, file))
            aud_input_set_tuple (tuple.ref ());

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
            aud_input_write_audio (audio, sizeof (float) * info.samples);
    }

    NeAACDecClose (decoder);
    return true;

ERR_CLOSE_DECODER:
    NeAACDecClose (decoder);
    return false;
}

#define AUD_PLUGIN_NAME        N_("AAC (Raw) Decoder")
#define AUD_INPUT_PLAY         my_decode_aac
#define AUD_INPUT_IS_OUR_FILE  parse_aac_stream
#define AUD_INPUT_READ_TUPLE   aac_get_tuple
#define AUD_INPUT_EXTS         fmts

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
