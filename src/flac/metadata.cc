/*
 * A FLAC decoder plugin for the Audacious Media Player
 * Copyright (C) 2005 Ralf Ertzinger
 * Copyright (C) 2010 John Lindgren
 * Copyright (C) 2010 Michał Lipski <tallica@o2.pl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdlib.h>
#include <string.h>

#define WANT_VFS_STDIO_COMPAT
#include <libaudcore/runtime.h>
#include <libaudcore/i18n.h>
#include <libaudcore/audstrings.h>

#include "flacng.h"

static size_t read_cb(void *ptr, size_t size, size_t nmemb, FLAC__IOHandle handle)
{
    int64_t read;

    if (handle == nullptr)
    {
        AUDERR("Trying to read data from an uninitialized file!\n");
        return -1;
    }

    read = ((VFSFile *) handle)->fread (ptr, size, nmemb);

    switch (read)
    {
        case -1:
            AUDERR("Error while reading from stream!\n");
            return -1;

        case 0:
            AUDDBG("Stream reached EOF\n");
            return 0;

        default:
            return read;
    }
}

static size_t write_cb(const void *ptr, size_t size, size_t nmemb, FLAC__IOHandle handle)
{
    return ((VFSFile *) handle)->fwrite (ptr, size, nmemb);
}

static int seek_cb(FLAC__IOHandle handle, FLAC__int64 offset, int whence)
{
    if (((VFSFile *) handle)->fseek (offset, to_vfs_seek_type (whence)) != 0)
    {
        AUDERR("Could not seek to %ld!\n", (long)offset);
        return -1;
    }

    return 0;
}

static FLAC__int64 tell_cb(FLAC__IOHandle handle)
{
    int64_t offset;

    if ((offset = ((VFSFile *) handle)->ftell ()) < 0)
    {
        AUDERR("Could not tell current position!\n");
        return -1;
    }

    AUDDBG ("Current position: %d\n", (int) offset);
    return offset;
}

static int eof_cb(FLAC__IOHandle handle)
{
    return ((VFSFile *) handle)->feof ();
}

static FLAC__IOCallbacks io_callbacks = {
    read_cb,
    write_cb,
    seek_cb,
    tell_cb,
    eof_cb,
    nullptr
};

static void insert_str_tuple_to_vc (FLAC__StreamMetadata * vc_block,
 const Tuple & tuple, Tuple::Field field, const char * field_name)
{
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    String val = tuple.get_str (field);

    if (! val)
        return;

    StringBuf str = str_printf ("%s=%s", field_name, (const char *) val);
    entry.entry = (FLAC__byte *) (char *) str;
    entry.length = strlen(str);
    FLAC__metadata_object_vorbiscomment_insert_comment(vc_block,
        vc_block->data.vorbis_comment.num_comments, entry, true);
}

static void insert_int_tuple_to_vc (FLAC__StreamMetadata * vc_block,
 const Tuple & tuple, Tuple::Field field, const char * field_name)
{
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    int val = tuple.get_int (field);

    if (val <= 0)
        return;

    StringBuf str = str_printf ("%s=%d", field_name, val);
    entry.entry = (FLAC__byte *) (char *) str;
    entry.length = strlen(str);
    FLAC__metadata_object_vorbiscomment_insert_comment(vc_block,
        vc_block->data.vorbis_comment.num_comments, entry, true);
}

bool FLACng::write_tuple(const char *filename, VFSFile &file, const Tuple &tuple)
{
    AUDDBG("Update song tuple.\n");

    FLAC__Metadata_Iterator *iter;
    FLAC__Metadata_Chain *chain;
    FLAC__StreamMetadata *vc_block;
    FLAC__Metadata_ChainStatus status;

    chain = FLAC__metadata_chain_new();

    if (!FLAC__metadata_chain_read_with_callbacks(chain, &file, io_callbacks))
        goto ERR;

    iter = FLAC__metadata_iterator_new();

    FLAC__metadata_iterator_init(iter, chain);

    while (FLAC__metadata_iterator_next(iter))
        if (FLAC__metadata_iterator_get_block_type(iter) == FLAC__METADATA_TYPE_VORBIS_COMMENT)
        {
            FLAC__metadata_iterator_delete_block(iter, true);
            break;
        }

    vc_block = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);

    insert_str_tuple_to_vc(vc_block, tuple, Tuple::Title, "TITLE");
    insert_str_tuple_to_vc(vc_block, tuple, Tuple::Artist, "ARTIST");
    insert_str_tuple_to_vc(vc_block, tuple, Tuple::Album, "ALBUM");
    insert_str_tuple_to_vc(vc_block, tuple, Tuple::AlbumArtist, "ALBUMARTIST");
    insert_str_tuple_to_vc(vc_block, tuple, Tuple::Genre, "GENRE");
    insert_str_tuple_to_vc(vc_block, tuple, Tuple::Comment, "COMMENT");

    insert_int_tuple_to_vc(vc_block, tuple, Tuple::Year, "DATE");
    insert_int_tuple_to_vc(vc_block, tuple, Tuple::Track, "TRACKNUMBER");

    FLAC__metadata_iterator_insert_block_after(iter, vc_block);

    FLAC__metadata_iterator_delete(iter);
    FLAC__metadata_chain_sort_padding(chain);

    if (!FLAC__metadata_chain_write_with_callbacks(chain, true, &file, io_callbacks))
        goto ERR;

    FLAC__metadata_chain_delete(chain);
    return true;

ERR:
    status = FLAC__metadata_chain_status(chain);
    FLAC__metadata_chain_delete(chain);

    AUDERR("An error occured: %s\n", FLAC__Metadata_ChainStatusString[status]);
    return false;
}

static void add_text (Tuple & tuple, Tuple::Field field, const char * value)
{
    String cur = tuple.get_str (field);
    if (cur)
        tuple.set_str (field, str_concat ({cur, ", ", value}));
    else
        tuple.set_str (field, value);
}

static void parse_comment (Tuple & tuple, const char * key, const char * value)
{
    AUDDBG ("Found key %s <%s>\n", key, value);

    static const struct {
        const char * key;
        Tuple::Field field;
    } tfields[] = {
        {"ARTIST", Tuple::Artist},
        {"ALBUM", Tuple::Album},
        {"ALBUMARTIST", Tuple::AlbumArtist},
        {"TITLE", Tuple::Title},
        {"COMMENT", Tuple::Comment},
        {"GENRE", Tuple::Genre}
    };

    for (auto & tfield : tfields)
    {
        if (!strcmp_nocase(key, tfield.key))
        {
            add_text (tuple, tfield.field, value);
            return;
        }
    }

    if (!strcmp_nocase(key, "TRACKNUMBER"))
        tuple.set_int(Tuple::Track, atoi(value));
    else if (!strcmp_nocase(key, "DATE"))
        tuple.set_int(Tuple::Year, atoi(value));
    else if (!strcmp_nocase(key, "REPLAYGAIN_TRACK_GAIN"))
        tuple.set_gain(Tuple::TrackGain, Tuple::GainDivisor, value);
    else if (!strcmp_nocase(key, "REPLAYGAIN_TRACK_PEAK"))
        tuple.set_gain(Tuple::TrackPeak, Tuple::PeakDivisor, value);
    else if (!strcmp_nocase(key, "REPLAYGAIN_ALBUM_GAIN"))
        tuple.set_gain(Tuple::AlbumGain, Tuple::GainDivisor, value);
    else if (!strcmp_nocase(key, "REPLAYGAIN_ALBUM_PEAK"))
        tuple.set_gain(Tuple::AlbumPeak, Tuple::PeakDivisor, value);
}

bool FLACng::read_tag (const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image)
{
    AUDDBG("Probe for tuple.\n");

    FLAC__Metadata_Iterator *iter;
    FLAC__Metadata_Chain *chain;
    FLAC__StreamMetadata *metadata = nullptr;
    FLAC__Metadata_ChainStatus status;
    FLAC__StreamMetadata_VorbisComment_Entry *entry;
    char *key;
    char *value;

    tuple.set_str (Tuple::Codec, "Free Lossless Audio Codec (FLAC)");
    tuple.set_str (Tuple::Quality, _("lossless"));

    chain = FLAC__metadata_chain_new();

    if (!FLAC__metadata_chain_read_with_callbacks(chain, &file, io_callbacks))
        goto ERR;

    iter = FLAC__metadata_iterator_new();

    FLAC__metadata_iterator_init(iter, chain);

    do
    {
        switch (FLAC__metadata_iterator_get_block_type(iter))
        {
            case FLAC__METADATA_TYPE_VORBIS_COMMENT:
            {
                metadata = FLAC__metadata_iterator_get_block(iter);

                AUDDBG("Vorbis comment contains %d fields\n", metadata->data.vorbis_comment.num_comments);
                AUDDBG("Vendor string: %s\n", metadata->data.vorbis_comment.vendor_string.entry);

                entry = metadata->data.vorbis_comment.comments;

                for (unsigned i = 0; i < metadata->data.vorbis_comment.num_comments; i++, entry++)
                {
                    if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(*entry, &key, &value) == false)
                        AUDDBG("Could not parse comment\n");
                    else
                    {
                        parse_comment(tuple, key, value);
                        free(key);
                        free(value);
                    }
                }
                break;
            }

            case FLAC__METADATA_TYPE_STREAMINFO:
            {
                metadata = FLAC__metadata_iterator_get_block(iter);

                /* Calculate the stream length (milliseconds) */
                if (metadata->data.stream_info.sample_rate == 0)
                {
                    AUDERR("Invalid sample rate for stream!\n");
                    tuple.set_int (Tuple::Length, -1);
                }
                else
                {
                    tuple.set_int (Tuple::Length,
                        (metadata->data.stream_info.total_samples / metadata->data.stream_info.sample_rate) * 1000);
                    AUDDBG("Stream length: %d seconds\n", tuple.get_int (Tuple::Length));
                }

                int64_t size = file.fsize ();

                if (size < 0 || metadata->data.stream_info.total_samples == 0)
                    tuple.set_int (Tuple::Bitrate, 0);
                else
                {
                    int bitrate = 8 * size *
                        (int64_t) metadata->data.stream_info.sample_rate / metadata->data.stream_info.total_samples;

                    tuple.set_int (Tuple::Bitrate, (bitrate + 500) / 1000);
                }
                break;
            }

            case FLAC__METADATA_TYPE_PICTURE:
            {
                if (image && !image->len())
                {
                    metadata = FLAC__metadata_iterator_get_block(iter);

                    if (metadata->data.picture.type == FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER)
                    {
                        AUDDBG("FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER found.");
                        image->insert((const char *) metadata->data.picture.data, 0,
                         metadata->data.picture.data_length);
                    }
                }
                break;
            }

            default:
                ;
        }
    } while (FLAC__metadata_iterator_next(iter));

    FLAC__metadata_iterator_delete(iter);
    FLAC__metadata_chain_delete(chain);

    return true;

ERR:
    status = FLAC__metadata_chain_status(chain);
    FLAC__metadata_chain_delete(chain);

    AUDERR("An error occured: %s\n", FLAC__Metadata_ChainStatusString[status]);
    return false;
}
