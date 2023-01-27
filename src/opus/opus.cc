/*
 * Opus Decoder Plugin for Audacious
 * Copyright 2022 Thomas Lange
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <cstdlib>
#include <cstring>

#include <opus/opusfile.h>

#define WANT_VFS_STDIO_COMPAT
#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

class OpusPlugin : public InputPlugin
{
public:
    static const char about[];
    static const char * const exts[];
    static const char * const mimes[];

    static constexpr PluginInfo info = {
        N_("Opus Decoder"),
        PACKAGE,
        about
    };

    constexpr OpusPlugin() : InputPlugin(info, InputInfo()
        .with_exts(exts)
        .with_mimes(mimes)) {}

    bool is_our_file(const char * filename, VFSFile & file);
    bool read_tag(const char * filename, VFSFile & file, Tuple & tuple,
                  Index<char> * image);
    bool play(const char * filename, VFSFile & file);

private:
    static const int pcm_frames = 1024;
    static const int pcm_bufsize = 2 * pcm_frames;
    static const int sample_rate = 48000; /* Opus supports 48 kHz only */

    int m_bitrate = 0;
    int m_channels = 0;
};

EXPORT OpusPlugin aud_plugin_instance;

static int opcb_read(void * stream, unsigned char * buf, int size)
{
    VFSFile * file = static_cast<VFSFile *>(stream);
    return file->fread(buf, 1, size);
}

static int opcb_seek(void * stream, opus_int64 offset, int whence)
{
    VFSFile * file = static_cast<VFSFile *>(stream);
    return file->fseek(offset, to_vfs_seek_type(whence));
}

static opus_int64 opcb_tell(void * stream)
{
    VFSFile * file = static_cast<VFSFile *>(stream);
    return file->ftell();
}

static OggOpusFile * open_file(VFSFile & file)
{
    bool stream = file.fsize() < 0;

    OpusFileCallbacks opus_callbacks = {
        opcb_read,
        stream ? nullptr : opcb_seek,
        stream ? nullptr : opcb_tell,
        nullptr,
    };

    return op_open_callbacks(&file, &opus_callbacks, nullptr, 0, nullptr);
}

static void read_tags(const OpusTags * tags, Tuple & tuple)
{
    const char * title = opus_tags_query(tags, "TITLE", 0);
    const char * artist = opus_tags_query(tags, "ARTIST", 0);
    const char * album = opus_tags_query(tags, "ALBUM", 0);
    const char * album_artist = opus_tags_query(tags, "ALBUMARTIST", 0);
    const char * genre = opus_tags_query(tags, "GENRE", 0);
    const char * comment = opus_tags_query(tags, "COMMENT", 0);
    const char * description = opus_tags_query(tags, "DESCRIPTION", 0);
    const char * music_brainz_id = opus_tags_query(tags, "musicbrainz_trackid", 0);
    const char * publisher = opus_tags_query(tags, "publisher", 0);
    const char * catalog_num = opus_tags_query(tags, "CATALOGNUMBER", 0);
    const char * track = opus_tags_query(tags, "TRACKNUMBER", 0);
    const char * date = opus_tags_query(tags, "DATE", 0);

    tuple.set_str(Tuple::Title, title);
    tuple.set_str(Tuple::Artist, artist);
    tuple.set_str(Tuple::Album, album);
    tuple.set_str(Tuple::AlbumArtist, album_artist);
    tuple.set_str(Tuple::Genre, genre);
    tuple.set_str(Tuple::Comment, comment);
    tuple.set_str(Tuple::Description, description);
    tuple.set_str(Tuple::MusicBrainzID, music_brainz_id);
    tuple.set_str(Tuple::Publisher, publisher);
    tuple.set_str(Tuple::CatalogNum, catalog_num);

    if (track)
        tuple.set_int(Tuple::Track, std::atoi(track));
    if (date)
        tuple.set_int(Tuple::Year, std::atoi(date));
}

static Index<char> read_image_from_tags(const OpusTags * tags,
                                        const char * filename)
{
    Index<char> data;

    const char * image = opus_tags_query(tags, "METADATA_BLOCK_PICTURE", 0);
    if (!image)
        return data;

    auto picture_tag = SmartPtr<OpusPictureTag>(new OpusPictureTag);
    opus_picture_tag_init(picture_tag.get());

    if (opus_picture_tag_parse(picture_tag.get(), image) < 0)
    {
        AUDERR("Error parsing METADATA_BLOCK_PICTURE in %s.\n", filename);
        return data;
    }

    if (picture_tag->format == OP_PIC_FORMAT_JPEG ||
        picture_tag->format == OP_PIC_FORMAT_PNG ||
        picture_tag->format == OP_PIC_FORMAT_GIF)
    {
        data.insert(reinterpret_cast<const char *>(picture_tag->data), 0,
                    picture_tag->data_length);
    }

    return data;
}

static bool update_tuple(OggOpusFile * opus_file, Tuple & tuple)
{
    const OpusTags * tags = op_tags(opus_file, -1);
    if (!tags)
        return false;

    String old_title = tuple.get_str(Tuple::Title);
    const char * new_title = opus_tags_query(tags, "TITLE", 0);

    if (!new_title || (old_title && !std::strcmp(old_title, new_title)))
        return false;

    read_tags(tags, tuple);
    return true;
}

static bool update_replay_gain(OggOpusFile * opus_file,
                               ReplayGainInfo * rg_info)
{
    const OpusTags * tags = op_tags(opus_file, -1);
    if (!tags)
        return false;

    const char * album_gain = opus_tags_query(tags, "REPLAYGAIN_ALBUM_GAIN", 0);
    const char * track_gain = opus_tags_query(tags, "REPLAYGAIN_TRACK_GAIN", 0);

    /* stop if we have no gain values */
    if (!album_gain && !track_gain)
        return false;

    /* fill in missing values if we can */
    if (!album_gain)
        album_gain = track_gain;
    if (!track_gain)
        track_gain = album_gain;

    rg_info->album_gain = str_to_double(album_gain);
    rg_info->track_gain = str_to_double(track_gain);

    const char * album_peak = opus_tags_query(tags, "REPLAYGAIN_ALBUM_PEAK", 0);
    const char * track_peak = opus_tags_query(tags, "REPLAYGAIN_TRACK_PEAK", 0);

    if (!album_peak && !track_peak)
    {
        /* okay, we have gain but no peak values */
        rg_info->album_peak = 0;
        rg_info->track_peak = 0;
    }
    else
    {
        /* fill in missing values if we can */
        if (!album_peak)
            album_peak = track_peak;
        if (!track_peak)
            track_peak = album_peak;

        rg_info->album_peak = str_to_double(album_peak);
        rg_info->track_peak = str_to_double(track_peak);
    }

    AUDDBG("Album gain: %s (%f)\n", album_gain, rg_info->album_gain);
    AUDDBG("Track gain: %s (%f)\n", track_gain, rg_info->track_gain);
    AUDDBG("Album peak: %s (%f)\n", album_peak, rg_info->album_peak);
    AUDDBG("Track peak: %s (%f)\n", track_peak, rg_info->track_peak);

    return true;
}

bool OpusPlugin::is_our_file(const char * filename, VFSFile & file)
{
    char buf[36];

    if (file.fread(buf, 1, sizeof buf) != sizeof buf)
        return false;

    if (!std::strncmp(buf, "OggS", 4))
    {
        if (!std::strncmp(buf + 28, "OpusHead", 8))
            return true;
    }

    return false;
}

bool OpusPlugin::read_tag(const char * filename, VFSFile & file, Tuple & tuple,
                          Index<char> * image)
{
    OggOpusFile * opus_file = open_file(file);
    if (!opus_file)
    {
        AUDERR("Failed to open Opus file\n");
        return false;
    }

    m_channels = op_channel_count(opus_file, -1);
    m_bitrate = op_bitrate(opus_file, -1);
    tuple.set_format("Opus", m_channels, sample_rate, m_bitrate / 1000);

    int total_time = op_pcm_total(opus_file, -1);
    if (total_time > 0)
        tuple.set_int(Tuple::Length, total_time / (sample_rate / 1000));

    const OpusTags * tags = op_tags(opus_file, -1);
    if (tags)
        read_tags(tags, tuple);
    if (image && tags)
        * image = read_image_from_tags(tags, filename);

    op_free(opus_file);
    return true;
}

bool OpusPlugin::play(const char * filename, VFSFile & file)
{
    OggOpusFile * opus_file = open_file(file);
    if (!opus_file)
    {
        AUDERR("Failed to open Opus file\n");
        return false;
    }

    Index<float> pcm_out;
    pcm_out.resize(pcm_bufsize * sizeof(float));

    bool error = false;
    int last_section = -1;
    Tuple tuple = get_playback_tuple();
    ReplayGainInfo rg_info;

    set_stream_bitrate(m_bitrate);

    if (update_tuple(opus_file, tuple))
        set_playback_tuple(tuple.ref());

    if (update_replay_gain(opus_file, &rg_info))
        set_replay_gain(rg_info);

    open_audio(FMT_FLOAT, sample_rate, m_channels);

    while (!check_stop())
    {
        int seek_value = check_seek();

        if (seek_value >= 0 &&
            op_pcm_seek(opus_file, seek_value * (sample_rate / 1000)) < 0)
        {
            AUDERR("Failed to seek Opus file\n");
            error = true;
            break;
        }

        int current_section = last_section;
        int bytes = op_read_float(opus_file, pcm_out.begin(), pcm_frames,
                                  &current_section);
        if (bytes == OP_HOLE)
            continue;

        if (bytes <= 0)
            break;

        if (update_tuple(opus_file, tuple))
            set_playback_tuple(tuple.ref());

        if (current_section != last_section)
        {
            int channels = op_channel_count(opus_file, -1);

            if (channels != m_channels)
            {
                m_channels = channels;

                if (update_replay_gain(opus_file, &rg_info))
                    set_replay_gain(rg_info);

                open_audio(FMT_FLOAT, sample_rate, m_channels);
            }
        }

        write_audio(pcm_out.begin(), bytes * m_channels * sizeof(float));

        if (current_section != last_section)
        {
            m_bitrate = op_bitrate(opus_file, -1);
            set_stream_bitrate(m_bitrate);

            last_section = current_section;
        }
    }

    op_free(opus_file);
    return !error;
}

const char OpusPlugin::about[] =
 N_("Opus Decoder Plugin for Audacious\n"
    "Copyright 2022 Thomas Lange");

const char * const OpusPlugin::exts[] = {"opus", nullptr};

const char * const OpusPlugin::mimes[] = {
    "application/ogg",
    "audio/ogg",
    "audio/opus",
    "audio/x-opus+ogg",
    nullptr
};
