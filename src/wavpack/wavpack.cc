#include <stdlib.h>
#include <string.h>

#include <wavpack/wavpack.h>

#define WANT_VFS_STDIO_COMPAT
#include <audacious/audtag.h>
#include <libaudcore/runtime.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>

#define BUFFER_SIZE 256 /* read buffer size, in samples / frames */
#define SAMPLE_SIZE(a) (a == 8 ? sizeof(uint8_t) : (a == 16 ? sizeof(uint16_t) : sizeof(uint32_t)))
#define SAMPLE_FMT(a) (a == 8 ? FMT_S8 : (a == 16 ? FMT_S16_NE : (a == 24 ? FMT_S24_NE : FMT_S32_NE)))

class WavpackPlugin : public InputPlugin
{
public:
    static const char about[];
    static const char * const exts[];

    static constexpr PluginInfo info = {
        N_("WavPack Decoder"),
        PACKAGE,
        about
    };

    constexpr WavpackPlugin() : InputPlugin (info, InputInfo (FlagWritesTag)
        .with_exts (exts)) {}

    bool is_our_file (const char * filename, VFSFile & file)
        { return false; }

    bool read_tag (const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image);
    bool write_tuple (const char * filename, VFSFile & file, const Tuple & tuple);
    bool play (const char * filename, VFSFile & file);
};

EXPORT WavpackPlugin aud_plugin_instance;

/* Audacious VFS wrappers for Wavpack stream reading
 */

static int32_t
wv_read_bytes(void *id, void *data, int32_t bcount)
{
    return ((VFSFile *) id)->fread (data, 1, bcount);
}

static uint32_t
wv_get_pos(void *id)
{
    return aud::clamp (((VFSFile *) id)->ftell (), (int64_t) 0, (int64_t) 0xffffffff);
}

static int
wv_set_pos_abs(void *id, uint32_t pos)
{
    return ((VFSFile *) id)->fseek (pos, VFS_SEEK_SET);
}

static int
wv_set_pos_rel(void *id, int32_t delta, int mode)
{
    return ((VFSFile *) id)->fseek (delta, to_vfs_seek_type(mode));
}

static int
wv_push_back_byte(void *id, int c)
{
    return (((VFSFile *) id)->fseek (-1, VFS_SEEK_CUR) == 0) ? c : -1;
}

static uint32_t
wv_get_length(void *id)
{
    return aud::clamp (((VFSFile *) id)->fsize (), (int64_t) 0, (int64_t) 0xffffffff);
}

static int wv_can_seek(void *id)
{
    return (((VFSFile *) id)->fsize () >= 0);
}

static int32_t wv_write_bytes(void *id, void *data, int32_t bcount)
{
    return ((VFSFile *) id)->fwrite (data, 1, bcount);
}

WavpackStreamReader wv_readers = {
    wv_read_bytes,
    wv_get_pos,
    wv_set_pos_abs,
    wv_set_pos_rel,
    wv_push_back_byte,
    wv_get_length,
    wv_can_seek,
    wv_write_bytes
};

static bool wv_attach (const char * filename, VFSFile & wv_input,
 VFSFile & wvc_input, WavpackContext * * ctx, char * error, int flags)
{
    if (flags & OPEN_WVC)
    {
        StringBuf corrFilename = str_concat ({filename, "c"});
        if (VFSFile::test_file (corrFilename, VFS_IS_REGULAR))
            wvc_input = VFSFile (corrFilename, "r");
    }

    * ctx = WavpackOpenFileInputEx (& wv_readers, & wv_input,
     wvc_input ? (& wvc_input) : nullptr, error, flags, 0);

    return (* ctx != nullptr);
}

static void wv_deattach (WavpackContext * ctx)
{
    WavpackCloseFile(ctx);
}

bool WavpackPlugin::play (const char * filename, VFSFile & file)
{
    int sample_rate, num_channels, bits_per_sample;
    unsigned num_samples;
    WavpackContext *ctx = nullptr;
    VFSFile wvc_input;

    if (! wv_attach (filename, file, wvc_input, & ctx, nullptr, OPEN_TAGS | OPEN_WVC))
    {
        AUDERR ("Error opening Wavpack file '%s'.", filename);
        return false;
    }

    sample_rate = WavpackGetSampleRate(ctx);
    num_channels = WavpackGetNumChannels(ctx);
    bits_per_sample = WavpackGetBitsPerSample(ctx);
    num_samples = WavpackGetNumSamples(ctx);

    set_stream_bitrate(WavpackGetAverageBitrate(ctx, num_channels));
    open_audio(SAMPLE_FMT(bits_per_sample), sample_rate, num_channels);

    Index<int32_t> input;
    input.resize (BUFFER_SIZE * num_channels);

    Index<char> output;
    output.resize (BUFFER_SIZE * num_channels * SAMPLE_SIZE (bits_per_sample));

    while (! check_stop ())
    {
        int seek_value = check_seek ();
        if (seek_value >= 0)
            WavpackSeekSample (ctx, (int64_t) seek_value * sample_rate / 1000);

        /* Decode audio data */
        unsigned samples_left = num_samples - WavpackGetSampleIndex(ctx);

        if (samples_left == 0)
            break;

        int ret = WavpackUnpackSamples (ctx, input.begin (), BUFFER_SIZE);

        if (ret < 0)
        {
            AUDERR ("Error decoding file.\n");
            break;
        }
        else
        {
            /* Perform audio data conversion and output */
            int32_t * rp = input.begin ();
            int8_t * wp = (int8_t *) output.begin ();
            int16_t * wp2 = (int16_t *) output.begin ();
            int32_t * wp4 = (int32_t *) output.begin ();

            if (bits_per_sample == 8)
            {
                for (int i = 0; i < ret * num_channels; i++, wp++, rp++)
                    *wp = *rp & 0xff;
            }
            else if (bits_per_sample == 16)
            {
                for (int i = 0; i < ret * num_channels; i++, wp2++, rp++)
                    *wp2 = *rp & 0xffff;
            }
            else if (bits_per_sample == 24 || bits_per_sample == 32)
            {
                for (int i = 0; i < ret * num_channels; i++, wp4++, rp++)
                    *wp4 = *rp;
            }

            write_audio (output.begin (),
             ret * num_channels * SAMPLE_SIZE (bits_per_sample));
        }
    }

    wv_deattach (ctx);
    return true;
}

static StringBuf
wv_get_quality(WavpackContext *ctx)
{
    int mode = WavpackGetMode(ctx);
    const char *quality;

    if (mode & MODE_LOSSLESS)
        quality = _("lossless");
    else if (mode & MODE_HYBRID)
        quality = _("lossy (hybrid)");
    else
        quality = _("lossy");

    return str_concat ({quality, (mode & MODE_WVC) ? " (wvc corrected)" : "",
     (mode & MODE_DNS) ? " (dynamic noise shaped)" : ""});
}

bool WavpackPlugin::read_tag (const char * filename, VFSFile & file, Tuple & tuple,
 Index<char> * image)
{
    char error[1024];

    auto ctx = WavpackOpenFileInputEx(&wv_readers, &file, nullptr, error, OPEN_TAGS, 0);
    if (! ctx)
        return false;

    AUDDBG("starting probe of %s\n", file.filename ());

    tuple.set_int (Tuple::Length,
        ((uint64_t) WavpackGetNumSamples(ctx) * 1000) / (uint64_t) WavpackGetSampleRate(ctx));
    tuple.set_str (Tuple::Codec, "WavPack");

    tuple.set_str (Tuple::Quality, wv_get_quality (ctx));

    WavpackCloseFile(ctx);

    if (! file.fseek (0, VFS_SEEK_SET))
        audtag::read_tag (file, tuple, nullptr);

    AUDDBG("returning tuple for file %s\n", file.filename ());
    return true;
}

bool WavpackPlugin::write_tuple (const char * filename, VFSFile & handle, const Tuple & tuple)
{
    return audtag::write_tuple (handle, tuple, audtag::TagType::APE);
}

const char WavpackPlugin::about[] =
 N_("Copyright 2006 William Pitcock <nenolod@nenolod.net>\n\n"
    "Some of the plugin code was by Miles Egan.");

const char * const WavpackPlugin::exts[] = { "wv", nullptr };
