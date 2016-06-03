#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <neaacdec.h>

#include <audacious/audtag.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

class AACDecoder : public InputPlugin
{
public:
    static const char * const exts[];

    static constexpr PluginInfo info = {
        N_("AAC (Raw) Decoder"),
        PACKAGE
    };

    constexpr AACDecoder () : InputPlugin (info, InputInfo ()
        .with_exts (exts)) {}

    bool is_our_file (const char * filename, VFSFile & file);
    bool read_tag (const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image);
    bool play (const char * filename, VFSFile & file);
};

EXPORT AACDecoder aud_plugin_instance;

const char * const AACDecoder::exts[] = {"aac", nullptr};

/*
 * BUFFER_SIZE is the highest amount of memory that can be pulled.
 * We use this for sanity checks, among other things, as mp4ff needs
 * a labotomy sometimes.
 */
#define BUFFER_SIZE (FAAD_MIN_STREAMSIZE * 16)

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

bool AACDecoder::is_our_file (const char * filename, VFSFile & stream)
{
    unsigned char data[8192];
    int offset, found, inner, size;

    size = 0;                   /* avoid bogus uninitialized variable warning */

    if (stream.fread (data, 1, sizeof data) != sizeof data)
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
static void calc_aac_info (VFSFile & handle, int * length, int * bitrate,
 int * samplerate, int * channels)
{
    NeAACDecHandle decoder;
    NeAACDecFrameInfo frame;
    bool initted = false;
    int size = handle.fsize ();
    unsigned char buffer[BUFFER_SIZE];
    int offset = 0, filled = 0;
    int found, bytes_used = 0, time_used = 0;

    decoder = nullptr;             /* avoid bogus uninitialized variable warning */

    *length = -1;
    *bitrate = -1;
    *samplerate = -1;
    *channels = -1;

    /* look for a representative bitrate in the middle of the file */
    if (size < 0 || handle.fseek (size / 2, VFS_SEEK_SET) < 0)
        goto DONE;

    for (found = 0; found < 32; found++)
    {
        if (filled < BUFFER_SIZE / 2)
        {
            memmove (buffer, buffer + offset, filled);
            offset = 0;

            if (handle.fread (buffer + filled, 1, BUFFER_SIZE - filled)
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

bool AACDecoder::read_tag (const char * filename, VFSFile & file, Tuple & tuple,
 Index<char> * image)
{
    int length, bitrate, samplerate, channels;

    tuple.set_str (Tuple::Codec, "MPEG-2/4 AAC");

    // TODO: error handling
    calc_aac_info (file, &length, &bitrate, &samplerate, &channels);

    if (length > 0)
        tuple.set_int (Tuple::Length, length);
    if (bitrate > 0)
        tuple.set_int (Tuple::Bitrate, bitrate);

    tuple.fetch_stream_info (file);

    return true;
}

static void aac_seek (VFSFile & file, NeAACDecHandle dec, int time, int len,
 void * buf, int size, int * buflen)
{
    /* == ESTIMATE BYTE OFFSET == */

    int64_t total = file.fsize ();
    if (total < 0)
    {
        AUDERR ("File is not seekable.\n");
        return;
    }

    /* == SEEK == */

    if (file.fseek (total * time / len, VFS_SEEK_SET))
        return;

    * buflen = file.fread (buf, 1, size);

    /* == FIND FRAME HEADER == */

    int used = aac_probe ((unsigned char *) buf, * buflen);

    if (used == * buflen)
    {
        AUDERR ("No valid frame header found.\n");
        * buflen = 0;
        return;
    }

    if (used)
    {
        * buflen -= used;
        memmove (buf, (char *) buf + used, * buflen);
        * buflen += file.fread ((char *) buf + * buflen, 1, size - * buflen);
    }

    /* == START DECODING == */

    unsigned char chan;
    unsigned long rate;

    if ((used = NeAACDecInit (dec, (unsigned char *) buf, * buflen, & rate, & chan)))
    {
        * buflen -= used;
        memmove (buf, (char *) buf + used, * buflen);
        * buflen += file.fread ((char *) buf + * buflen, 1, size - * buflen);
    }
}

bool AACDecoder::play (const char * filename, VFSFile & file)
{
    NeAACDecHandle decoder = 0;
    NeAACDecConfigurationPtr decoder_config;
    unsigned long samplerate = 0;
    unsigned char channels = 0;

    Tuple tuple = get_playback_tuple ();
    int bitrate = 1000 * aud::max (0, tuple.get_int (Tuple::Bitrate));

    if ((decoder = NeAACDecOpen ()) == nullptr)
    {
        AUDERR ("Open Decoder Error\n");
        return false;
    }

    decoder_config = NeAACDecGetCurrentConfiguration (decoder);
    decoder_config->outputFormat = FAAD_FMT_FLOAT;
    NeAACDecSetConfiguration (decoder, decoder_config);

    /* == FILL BUFFER == */

    unsigned char buf[BUFFER_SIZE];
    int buflen;
    buflen = file.fread (buf, 1, sizeof buf);

    /* == SKIP ID3 TAG == */

    if (buflen >= 10 && ! strncmp ((char *) buf, "ID3", 3))
    {
        int tagsize = 10 + (buf[6] << 21) + (buf[7] << 14) + (buf[8] << 7) + buf[9];

        if (file.fseek (tagsize, VFS_SEEK_SET))
        {
            AUDERR ("Failed to seek past ID3v2 tag.\n");
            goto ERR_CLOSE_DECODER;
        }

        buflen = file.fread (buf, 1, sizeof buf);
    }

    /* == FIND FRAME HEADER == */

    int used;
    used = aac_probe (buf, buflen);

    if (used == buflen)
    {
        AUDERR ("No valid frame header found.\n");
        goto ERR_CLOSE_DECODER;
    }

    if (used)
    {
        buflen -= used;
        memmove (buf, buf + used, buflen);
        buflen += file.fread (buf + buflen, 1, sizeof buf - buflen);
    }

    /* == START DECODING == */

    if ((used = NeAACDecInit (decoder, buf, buflen, & samplerate, & channels)))
    {
        buflen -= used;
        memmove (buf, buf + used, buflen);
        buflen += file.fread (buf + buflen, 1, sizeof buf - buflen);
    }

    /* == CHECK FOR METADATA == */

    if (tuple.fetch_stream_info (file))
        set_playback_tuple (tuple.ref ());

    set_stream_bitrate (bitrate);

    /* == START PLAYBACK == */

    open_audio (FMT_FLOAT, samplerate, channels);

    /* == MAIN LOOP == */

    while (! check_stop ())
    {
        /* == HANDLE SEEK REQUESTS == */

        int seek_value = check_seek ();

        if (seek_value >= 0)
        {
            int length = tuple.get_int (Tuple::Length);
            if (length > 0)
                aac_seek (file, decoder, seek_value, length, buf, sizeof buf, & buflen);
        }

        /* == CHECK FOR END OF FILE == */

        if (! buflen)
            break;

        /* == CHECK FOR METADATA == */

        if (tuple.fetch_stream_info (file))
            set_playback_tuple (tuple.ref ());

        /* == DECODE A FRAME == */

        NeAACDecFrameInfo info;
        void * audio = NeAACDecDecode (decoder, & info, buf, buflen);

        if (info.error)
        {
            AUDERR ("%s.\n", NeAACDecGetErrorMessage (info.error));

            if (buflen)
            {
                used = 1 + aac_probe (buf + 1, buflen - 1);
                buflen -= used;
                memmove (buf, buf + used, buflen);
                buflen += file.fread (buf + buflen, 1, sizeof buf - buflen);
            }

            continue;
        }

        if ((used = info.bytesconsumed))
        {
            buflen -= used;
            memmove (buf, buf + used, buflen);
            buflen += file.fread (buf + buflen, 1, sizeof buf - buflen);
        }

        /* == PLAY THE SOUND == */

        if (audio && info.samples)
            write_audio (audio, sizeof (float) * info.samples);
    }

    NeAACDecClose (decoder);
    return true;

ERR_CLOSE_DECODER:
    NeAACDecClose (decoder);
    return false;
}
