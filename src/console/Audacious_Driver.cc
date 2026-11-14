/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2005-2009 Audacious Team
 *
 * Driver for Game_Music_Emu library. See details at:
 * http://www.slack.net/~ant/libs/
 */

#include <cstring>
#include <math.h>
#include <vector>
#include <zlib.h>

#include <gme/gme.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>

#include "configure.h"
#include "plugin.h"

static const int fade_threshold = 10 * 1000;
static const int fade_length    = 8 * 1000;

static bool log_err(gme_err_t err)
{
    if (err)
        AUDERR("%s\n", err);
    return !!err;
}

static void log_warning(Music_Emu * emu)
{
    const char *str = gme_warning(emu);
    if (str)
        AUDWARN("%s\n", str);
}

/* Gzip-compressed formats such as VGZ are inflated here. libgme's
 * in-memory loader does not decompress the file itself. */
static bool inflate_gzip(Index<char> &data)
{
    if (data.len() < 2)
        return true;

    const unsigned char *header = (const unsigned char *)data.begin();
    if (header[0] != 0x1f || header[1] != 0x8b)
        return true;

    z_stream strm;
    std::memset(&strm, 0, sizeof strm);
    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK)
        return false;

    if (data.len() > (int)UINT_MAX)
    {
        inflateEnd(&strm);
        return false;
    }

    std::vector<unsigned char> out(data.len() * 4 + 64);
    strm.next_in = (Bytef *)data.begin();
    strm.avail_in = (uInt)data.len();

    int ret;
    do
    {
        if (strm.total_out >= out.size())
            out.resize(out.size() * 2);

        strm.next_out = out.data() + strm.total_out;
        strm.avail_out = (uInt)(out.size() - strm.total_out);
        ret = inflate(&strm, Z_NO_FLUSH);
    }
    while (ret == Z_OK);

    unsigned long size = strm.total_out;
    inflateEnd(&strm);

    if (ret != Z_STREAM_END || size > (unsigned long)INT_MAX)
        return false;

    Index<char> inflated;
    inflated.insert((const char *)out.data(), 0, (int)size);
    data = std::move(inflated);
    return true;
}

/* Handles URL parsing, file opening and identification, and file loading. */
class ConsoleFileHandler {
public:
    String m_path;            // path without track number specification
    int m_track;             // track number (0 = first track)
    Music_Emu * m_emu;
    gme_type_t m_type;

    // Parses path and identifies file type
    ConsoleFileHandler(const char * path, VFSFile &fd);

    // Creates emulator and returns 0. If this wasn't a music file or
    // emulator couldn't be created, returns 1.
    int load(int sample_rate);

    // Deletes owned emu
    ~ConsoleFileHandler();

private:
    Index<char> m_data;
};

ConsoleFileHandler::ConsoleFileHandler(const char *path, VFSFile &fd)
{
    m_emu   = nullptr;
    m_type  = nullptr;
    m_track = -1;

    const char * sub;
    uri_parse (path, nullptr, nullptr, & sub, & m_track);
    m_path = String (str_copy (path, sub - path));

    m_track -= 1;

    m_data = fd.read_all();
    if (!inflate_gzip(m_data))
    {
        log_err("Corrupt gzip file");
        m_data.clear();
        return;
    }

    if (m_data.len() < 4)
        return;

    char header[4];
    std::memcpy(header, m_data.begin(), sizeof header);
    m_type = gme_identify_extension(gme_identify_header(header));
    if (!m_type)
    {
        m_type = gme_identify_extension(m_path);
        if (m_type != gme_gym_type) // only trust file extension for headerless .gym files
            m_type = nullptr;
    }
}

ConsoleFileHandler::~ConsoleFileHandler()
{
    gme_delete(m_emu);
}

int ConsoleFileHandler::load(int sample_rate)
{
    if (!m_type)
        return 1;

    m_emu = gme_new_emu(m_type, sample_rate);
    if (m_emu == nullptr)
    {
        log_err("Out of memory allocating emulator engine. Fatal error.");
        return 1;
    }

    if (log_err(gme_load_data(m_emu, m_data.begin(), m_data.len())))
        return 1;

    m_data.clear();
    log_warning(m_emu);

    // load companion .m3u or .m3u8 from same directory
    StringBuf base_path = str_copy(m_path);
    const char *dot = strrchr(base_path, '.');
    if (dot)
        base_path.resize(dot - base_path);

    // Check for existence before opening.
    StringBuf m3u_path = str_concat({base_path, ".m3u8"});
    if (!VFSFile::test_file(m3u_path, VFS_EXISTS))
        m3u_path = str_concat({base_path, ".m3u"});

    if (VFSFile::test_file(m3u_path, VFS_EXISTS))
    {
        VFSFile m3u(m3u_path, "r");
        if (m3u)
        {
            Index<char> text = m3u.read_all();
            if (text.len() > 0 && log_err(gme_load_m3u_data(m_emu, text.begin(), text.len())))
                log_warning(m_emu);
        }
    }

    return 0;
}

static int get_track_length(const gme_info_t &info)
{
    int length = info.length;
    if (length <= 0)
        length = info.intro_length + 2 * info.loop_length;

    if (length <= 0)
        length = audcfg.loop_length * 1000;
    else if (length >= fade_threshold)
        length += fade_length;

    return length;
}

bool ConsolePlugin::read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image)
{
    ConsoleFileHandler fh(filename, file);

    if (!fh.m_type)
        return false;

    if (fh.load(gme_info_only))
        return false;

    int track = fh.m_track < 0 ? 0 : fh.m_track;
    gme_info_t *info = nullptr;
    if (log_err(gme_track_info(fh.m_emu, &info, track)))
        return false;

    auto set_str = [&tuple](Tuple::Field f, const char *s)
        { if (s[0]) tuple.set_str(f, s); };

    set_str(Tuple::Artist, info->author);
    set_str(Tuple::Album, info->game);
    set_str(Tuple::Title, info->song);
    set_str(Tuple::Copyright, info->copyright);
    set_str(Tuple::Codec, info->system);
    set_str(Tuple::Comment, info->comment);

    int track_count = gme_track_count(fh.m_emu);
    if (fh.m_track >= 0)
    {
        tuple.set_int(Tuple::Track, fh.m_track + 1);
        tuple.set_int(Tuple::Subtune, fh.m_track + 1);
        tuple.set_int(Tuple::NumSubtunes, track_count);
    }
    else
        tuple.set_subtunes(track_count, nullptr);

    tuple.set_int (Tuple::Length, get_track_length (*info));
    tuple.set_int (Tuple::Channels, 2);

    gme_free_info(info);
    return true;
}

bool ConsolePlugin::play(const char *filename, VFSFile &file)
{
    int length, sample_rate;

    // identify file
    ConsoleFileHandler fh(filename, file);
    if (!fh.m_type)
        return false;

    if (fh.m_track < 0)
        fh.m_track = 0;

    // select sample rate
    sample_rate = 0;
    if (fh.m_type == gme_spc_type)
        sample_rate = 32000;
    if (audcfg.resample)
        sample_rate = audcfg.resample_rate;
    if (sample_rate == 0)
        sample_rate = 44100;

    // create emulator and load file
    if (fh.load(sample_rate))
        return false;

    // stereo echo depth
    gme_set_stereo_depth(fh.m_emu, 1.0 / 100 * audcfg.echo);

    // set equalizer
    if (audcfg.treble || audcfg.bass)
    {
        gme_equalizer_t eq = {};

        // bass - logarithmic, 2 to 8194 Hz
        double bass = 1.0 - (audcfg.bass / 200.0 + 0.5);
        eq.bass = (long) (2.0 + pow( 2.0, bass * 13 ));

        // treble - -50 to 0 to +5 dB
        double treble = audcfg.treble / 100.0;
        eq.treble = treble * (treble < 0 ? 50.0 : 5.0);

        gme_set_equalizer(fh.m_emu, &eq);
    }

    // get info
    length = -1;
    gme_info_t *info = nullptr;
    if (!log_err(gme_track_info(fh.m_emu, &info, fh.m_track)))
    {
        if (fh.m_type == gme_spc_type && audcfg.ignore_spc_length)
            info->length = -1;

        length = get_track_length(*info);
        set_stream_bitrate(gme_voice_count(fh.m_emu) * 1000);
        gme_free_info(info);
    }

    // start track
    if (log_err(gme_start_track(fh.m_emu, fh.m_track)))
        return false;

    log_warning(fh.m_emu);

    open_audio(FMT_S16_NE, sample_rate, 2);

    // set fade time. libgme fades over 8000 ms, matching fade_length.
    if (length <= 0)
        length = audcfg.loop_length * 1000;
    if (length >= fade_threshold + fade_length)
        length -= fade_length / 2;
    gme_set_fade(fh.m_emu, length);

    while (!check_stop())
    {
        /* Perform seek, if requested */
        int seek_value = check_seek();
        if (seek_value >= 0)
            log_err(gme_seek(fh.m_emu, seek_value));

        /* Fill and play buffer of audio */
        int const buf_size = 1024;
        short buf[buf_size];

        if (log_err(gme_play(fh.m_emu, buf_size, buf)))
            break;

        write_audio(buf, sizeof buf);

        if (gme_track_ended(fh.m_emu))
            break;
    }

    return true;
}
