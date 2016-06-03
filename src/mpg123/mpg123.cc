/*
 * Copyright (c) 2010 William Pitcock <nenolod@dereferenced.org>.
 * Copyright (c) 2010-2011 John Lindgren <john.lindgren@tds.net>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>

#undef EXPORT
#include <mpg123.h>

// mpg123.h redefines EXPORT
#undef EXPORT
#include "config.h"

#ifdef DEBUG_MPG123_IO
# define MPG123_IODBG(...)  AUDDBG(__VA_ARGS__)
#else
# define MPG123_IODBG(...)  do { } while (0)
#endif

#define WANT_VFS_STDIO_COMPAT
#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <audacious/audtag.h>

class MPG123Plugin : public InputPlugin
{
public:
    static const char * const exts[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("MPG123 Plugin"),
        PACKAGE,
        nullptr,
        & prefs
    };

    constexpr MPG123Plugin() : InputPlugin (info, InputInfo (FlagWritesTag)
        .with_exts (exts)) {}

    bool init ();
    void cleanup ();

    bool is_our_file (const char * filename, VFSFile & file);
    bool read_tag (const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image);
    bool write_tuple (const char * filename, VFSFile & file, const Tuple & tuple);
    bool play (const char * filename, VFSFile & file);
};

EXPORT MPG123Plugin aud_plugin_instance;

const char * const MPG123Plugin::defaults[] = {
    "full_scan", "FALSE",
    nullptr
};

const PreferencesWidget MPG123Plugin::widgets[] = {
    WidgetLabel (N_("<b>Advanced</b>")),
    WidgetCheck (N_("Use accurate length calculation (slow)"),
        WidgetBool ("mpg123", "full_scan"))
};

const PluginPreferences MPG123Plugin::prefs = {{widgets}};

#define DECODE_OPTIONS (MPG123_QUIET | MPG123_GAPLESS | MPG123_SEEKBUFFER | MPG123_FUZZY)

// this is a macro so that the printed line number is meaningful
#define print_mpg123_error(filename, decoder) \
    AUDERR ("mpg123 error in %s: %s\n", filename, mpg123_strerror (decoder))

static ssize_t replace_read (void * file, void * buffer, size_t length)
{
    return ((VFSFile *) file)->fread (buffer, 1, length);
}

static off_t replace_lseek (void * file, off_t to, int whence)
{
    if (((VFSFile *) file)->fseek (to, to_vfs_seek_type (whence)) < 0)
        return -1;

    return ((VFSFile *) file)->ftell ();
}

static off_t replace_lseek_dummy (void * file, off_t to, int whence)
{
    return -1;
}

bool MPG123Plugin::init ()
{
    aud_config_set_defaults ("mpg123", defaults);

    AUDDBG("initializing mpg123 library\n");
    mpg123_init();

    return true;
}

void MPG123Plugin::cleanup ()
{
    AUDDBG("deinitializing mpg123 library\n");
    mpg123_exit();
}

struct DecodeState
{
    mpg123_handle * dec = nullptr;

    bool init (const char * filename, VFSFile & file, bool probing, bool stream);

    ~DecodeState()
        { mpg123_delete (dec); }

    long rate;
    int channels, encoding;
    mpg123_frameinfo info;
    size_t bytes_read;
    float buf[4096];
};

bool DecodeState::init (const char * filename, VFSFile & file, bool probing, bool stream)
{
    dec = mpg123_new (nullptr, nullptr);
    mpg123_param (dec, MPG123_ADD_FLAGS, DECODE_OPTIONS, 0);
    mpg123_replace_reader_handle (dec, replace_read,
     stream ? replace_lseek_dummy : replace_lseek, nullptr);

    /* be strict about junk data in file during content probe */
    if (probing)
        mpg123_param (dec, MPG123_RESYNC_LIMIT, 0, 0);

    mpg123_format_none (dec);

    for (int rate : {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000})
        mpg123_format (dec, rate, MPG123_MONO | MPG123_STEREO, MPG123_ENC_FLOAT_32);

    if (mpg123_open_handle (dec, & file) < 0)
        goto err;

    if (! stream && aud_get_bool ("mpg123", "full_scan") && mpg123_scan (dec) < 0)
        goto err;

    while (1)
    {
        if (mpg123_getformat (dec, & rate, & channels, & encoding) < 0)
            goto err;

        int ret = mpg123_read (dec, (unsigned char *) buf, sizeof buf, & bytes_read);

        if (ret == MPG123_NEW_FORMAT)
            continue;
        if (ret < 0)
            goto err;

        if (mpg123_info (dec, & info) < 0)
            goto err;

        return dec;
    }

err:
    if (probing)
        AUDDBG ("mpg123 error in %s: %s\n", filename, mpg123_strerror (dec));
    else
        AUDERR ("mpg123 error in %s: %s\n", filename, mpg123_strerror (dec));

    mpg123_delete (dec);
    dec = nullptr;

    return false;
}

// with better buffering in Audacious 3.7, this is now safe for streams
static bool detect_id3 (VFSFile & file)
{
    bool is_id3 = false;
    char id3buf[3];

    if (file.fread (id3buf, 1, 3) == 3)
        is_id3 = ! memcmp (id3buf, "ID3", 3);

    if (file.fseek (0, VFS_SEEK_SET) < 0)
        is_id3 = false;

    return is_id3;
}

static StringBuf make_format_string (const mpg123_frameinfo * info)
{
    static const char * vers[] = {"1", "2", "2.5"};
    return str_printf ("MPEG-%s layer %d", vers[info->version], info->layer);
}

bool MPG123Plugin::is_our_file (const char * filename, VFSFile & file)
{
    bool stream = (file.fsize () < 0);

    /* Some MP3s begin with enormous ID3 tags, which fill up the whole probe
     * buffer and thus hide any MP3 content.  As a workaround, assume that an
     * ID3 tag means an MP3 file.  --jlindgren */
    if (detect_id3 (file))
        return true;

    DecodeState s;
    if (! s.init (filename, file, true, stream))
        return false;

    AUDDBG ("Accepted as %s: %s.\n", (const char *) make_format_string (& s.info), filename);
    return true;
}

static bool read_mpg123_info (const char * filename, VFSFile & file, Tuple & tuple)
{
    int64_t size = file.fsize ();
    bool stream = (size < 0);

    DecodeState s;
    if (! s.init (filename, file, false, stream))
        return false;

    tuple.set_str (Tuple::Codec, make_format_string (& s.info));
    tuple.set_str (Tuple::Quality, str_printf ("%s, %d Hz", (s.channels == 2) ?
     _("Stereo") : (s.channels > 2) ? _("Surround") : _("Mono"), (int) s.rate));
    tuple.set_int (Tuple::Bitrate, s.info.bitrate);

    if (! stream)
    {
        int64_t samples = mpg123_length (s.dec);
        int length = (s.rate > 0) ? samples * 1000 / s.rate : 0;

        if (length > 0)
        {
            tuple.set_int (Tuple::Length, length);
            tuple.set_int (Tuple::Bitrate, 8 * size / length);
        }
    }

    return true;
}

bool MPG123Plugin::read_tag (const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image)
{
    bool stream = (file.fsize () < 0);

    if (! read_mpg123_info (filename, file, tuple))
        return false;

    if (stream)
        tuple.fetch_stream_info (file);
    else
    {
        if (file.fseek (0, VFS_SEEK_SET) != 0)
            return false;

        audtag::read_tag (file, tuple, image);
    }

    return true;
}

bool MPG123Plugin::play (const char * filename, VFSFile & file)
{
    bool stream = (file.fsize () < 0);

    Tuple tuple;
    if (stream)
    {
        tuple = get_playback_tuple ();
        if (detect_id3 (file) && audtag::read_tag (file, tuple, nullptr))
            set_playback_tuple (tuple.ref ());
    }

    DecodeState s;
    if (! s.init (filename, file, false, stream))
        return false;

    int bitrate = s.info.bitrate * 1000;
    int bitrate_sum = 0, bitrate_count = 0;
    int error_count = 0;

    set_stream_bitrate (bitrate);

    if (stream && tuple.fetch_stream_info (file))
        set_playback_tuple (tuple.ref ());

    open_audio (FMT_FLOAT, s.rate, s.channels);

    while (! check_stop ())
    {
        int seek = check_seek ();

        if (seek >= 0)
        {
            if (mpg123_seek (s.dec, (int64_t) seek * s.rate / 1000, SEEK_SET) < 0)
                print_mpg123_error (filename, s.dec);

            s.bytes_read = 0;
        }

        mpg123_info (s.dec, & s.info);
        bitrate_sum += s.info.bitrate;
        bitrate_count ++;

        if (bitrate_sum / bitrate_count != bitrate && bitrate_count >= 16)
        {
            set_stream_bitrate (bitrate_sum / bitrate_count * 1000);
            bitrate = bitrate_sum / bitrate_count;
            bitrate_sum = 0;
            bitrate_count = 0;
        }

        if (stream && tuple.fetch_stream_info (file))
            set_playback_tuple (tuple.ref ());

        if (! s.bytes_read)
        {
            int ret = mpg123_read (s.dec, (unsigned char *) s.buf, sizeof s.buf, & s.bytes_read);

            if (ret == MPG123_DONE || ret == MPG123_ERR_READER)
                break;

            if (ret < 0)
            {
                // log only the first error
                if (! error_count)
                    print_mpg123_error (filename, s.dec);

                // generally unreported errors are bad, but due to the number of
                // MP3s with junk data at the end, RESYNC_FAIL just becomes a
                // nuisance, so silence it
                if (++ error_count >= 10)
                    return (mpg123_errcode (s.dec) == MPG123_RESYNC_FAIL);
            }
        }

        if (s.bytes_read)
        {
            error_count = 0;

            write_audio (s.buf, s.bytes_read);
            s.bytes_read = 0;
        }
    }

    return true;
}

bool MPG123Plugin::write_tuple (const char * filename, VFSFile & file, const Tuple & tuple)
{
    if (file.fsize () < 0)  // stream?
        return false;

    return audtag::write_tuple (file, tuple, audtag::TagType::ID3v2);
}

const char * const MPG123Plugin::exts[] = { "mp3", "mp2", "mp1", "bmu", nullptr };
