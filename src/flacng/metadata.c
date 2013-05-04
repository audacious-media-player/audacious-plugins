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

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <audacious/debug.h>
#include <audacious/i18n.h>

#include "flacng.h"

static size_t read_cb(void *ptr, size_t size, size_t nmemb, FLAC__IOHandle handle)
{
    size_t read;

    if (handle == NULL)
    {
        FLACNG_ERROR("Trying to read data from an uninitialized file!\n");
        return -1;
    }

    read = vfs_fread(ptr, size, nmemb, handle);

    switch (read)
    {
        case -1:
            FLACNG_ERROR("Error while reading from stream!\n");
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
    return vfs_fwrite(ptr, size, nmemb, handle);
}

static int seek_cb(FLAC__IOHandle handle, FLAC__int64 offset, int whence)
{
    if (vfs_fseek(handle, offset, whence) != 0)
    {
        FLACNG_ERROR("Could not seek to %ld!\n", (long)offset);
        return -1;
    }

    return 0;
}

static FLAC__int64 tell_cb(FLAC__IOHandle handle)
{
    uint64_t offset;

    if ((offset = vfs_ftell(handle)) == -1)
    {
        FLACNG_ERROR("Could not tell current position!\n");
        return -1;
    }

    AUDDBG ("Current position: %d\n", (int) offset);
    return offset;
}

static int eof_cb(FLAC__IOHandle handle)
{
    return vfs_feof(handle);
}

static FLAC__IOCallbacks io_callbacks = {
    read_cb,
    write_cb,
    seek_cb,
    tell_cb,
    eof_cb,
    NULL
};

static void insert_str_tuple_to_vc (FLAC__StreamMetadata * vc_block,
 const Tuple * tuple, int tuple_name, char * field_name)
{
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    char *val = tuple_get_str(tuple, tuple_name, NULL);

    if (val == NULL)
        return;

    SPRINTF (str, "%s=%s", field_name, val);
    entry.entry = (FLAC__byte *) str;
    entry.length = strlen(str);
    FLAC__metadata_object_vorbiscomment_insert_comment(vc_block,
        vc_block->data.vorbis_comment.num_comments, entry, true);
    str_unref(val);
}

static void insert_int_tuple_to_vc (FLAC__StreamMetadata * vc_block,
 const Tuple * tuple, int tuple_name, char * field_name)
{
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    int val = tuple_get_int(tuple, tuple_name, NULL);

    if (val <= 0)
        return;

    SPRINTF (str, "%s=%d", field_name, val);
    entry.entry = (FLAC__byte *) str;
    entry.length = strlen(str);
    FLAC__metadata_object_vorbiscomment_insert_comment(vc_block,
        vc_block->data.vorbis_comment.num_comments, entry, true);
}

bool_t flac_update_song_tuple(const Tuple *tuple, VFSFile *fd)
{
    AUDDBG("Update song tuple.\n");

    FLAC__Metadata_Iterator *iter;
    FLAC__Metadata_Chain *chain;
    FLAC__StreamMetadata *vc_block;
    FLAC__Metadata_ChainStatus status;

    chain = FLAC__metadata_chain_new();

    if (!FLAC__metadata_chain_read_with_callbacks(chain, fd, io_callbacks))
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

    insert_str_tuple_to_vc(vc_block, tuple, FIELD_TITLE, "TITLE");
    insert_str_tuple_to_vc(vc_block, tuple, FIELD_ARTIST, "ARTIST");
    insert_str_tuple_to_vc(vc_block, tuple, FIELD_ALBUM, "ALBUM");
    insert_str_tuple_to_vc(vc_block, tuple, FIELD_GENRE, "GENRE");
    insert_str_tuple_to_vc(vc_block, tuple, FIELD_COMMENT, "COMMENT");

    insert_int_tuple_to_vc(vc_block, tuple, FIELD_YEAR, "DATE");
    insert_int_tuple_to_vc(vc_block, tuple, FIELD_TRACK_NUMBER, "TRACKNUMBER");

    FLAC__metadata_iterator_insert_block_after(iter, vc_block);

    FLAC__metadata_iterator_delete(iter);
    FLAC__metadata_chain_sort_padding(chain);

    if (!FLAC__metadata_chain_write_with_callbacks(chain, TRUE, fd, io_callbacks))
        goto ERR;

    FLAC__metadata_chain_delete(chain);
    return TRUE;

ERR:
    status = FLAC__metadata_chain_status(chain);
    FLAC__metadata_chain_delete(chain);

    FLACNG_ERROR("An error occured: %s\n", FLAC__Metadata_ChainStatusString[status]);
    return FALSE;
}

bool_t flac_get_image(const char *filename, VFSFile *fd, void **data, int64_t *length)
{
    AUDDBG("Probe for song image.\n");

    FLAC__Metadata_Iterator *iter;
    FLAC__Metadata_Chain *chain;
    FLAC__StreamMetadata *metadata = NULL;
    FLAC__Metadata_ChainStatus status;
    bool_t has_image = FALSE;

    chain = FLAC__metadata_chain_new();

    if (!FLAC__metadata_chain_read_with_callbacks(chain, fd, io_callbacks))
        goto ERR;

    iter = FLAC__metadata_iterator_new();

    FLAC__metadata_iterator_init(iter, chain);

    while (FLAC__metadata_iterator_next(iter))
        if (FLAC__metadata_iterator_get_block_type(iter) == FLAC__METADATA_TYPE_PICTURE)
            break;

    if (FLAC__metadata_iterator_get_block_type(iter) == FLAC__METADATA_TYPE_PICTURE)
    {
        metadata = FLAC__metadata_iterator_get_block(iter);

        if (metadata->data.picture.type == FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER)
        {
            AUDDBG("FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER found.");

            * data = malloc (metadata->data.picture.data_length);
            * length = metadata->data.picture.data_length;
            memcpy (* data, metadata->data.picture.data, * length);
            has_image = TRUE;
        }
    }

    FLAC__metadata_iterator_delete(iter);
    FLAC__metadata_chain_delete(chain);

    return has_image;

ERR:
    status = FLAC__metadata_chain_status(chain);
    FLAC__metadata_chain_delete(chain);

    FLACNG_ERROR("An error occured: %s\n", FLAC__Metadata_ChainStatusString[status]);
    return FALSE;
}

static void parse_gain_text(const char *text, int *value, int *unit)
{
    int sign = 1;

    *value = 0;
    *unit = 1;

    if (*text == '-')
    {
        sign = -1;
        text++;
    }

    while (*text >= '0' && *text <= '9')
    {
        *value = *value * 10 + (*text - '0');
        text++;
    }

    if (*text == '.')
    {
        text++;

        while (*text >= '0' && *text <= '9' && *value < INT_MAX / 10)
        {
            *value = *value * 10 + (*text - '0');
            *unit = *unit * 10;
            text++;
        }
    }

    *value = *value * sign;
}

static void set_gain_info(Tuple *tuple, int field, int unit_field, const char *text)
{
    int value, unit;

    parse_gain_text(text, &value, &unit);

    if (tuple_get_value_type(tuple, unit_field, NULL) == TUPLE_INT)
        value = value * (int64_t) tuple_get_int(tuple, unit_field, NULL) / unit;
    else
        tuple_set_int(tuple, unit_field, NULL, unit);

    tuple_set_int(tuple, field, NULL, value);
}

static void add_text (Tuple * tuple, int field, const char * value)
{
    char * cur = tuple_get_str (tuple, field, NULL);
    if (cur)
    {
        SPRINTF (both, "%s, %s", cur, value);
        tuple_set_str (tuple, field, NULL, both);
    }
    else
        tuple_set_str (tuple, field, NULL, value);

    str_unref(cur);
}

static void parse_comment (Tuple * tuple, const char * key, const char * value)
{
    AUDDBG ("Found key %s <%s>\n", key, value);

    const struct {
        const char * key;
        int field;
    } tfields[] = {
     {"ARTIST", FIELD_ARTIST},
     {"ALBUM", FIELD_ALBUM},
     {"TITLE", FIELD_TITLE},
     {"COMMENT", FIELD_COMMENT},
     {"GENRE", FIELD_GENRE}};

    for (int i = 0; i < sizeof tfields / sizeof tfields[0]; i ++)
    {
        if (! strcasecmp (key, tfields[i].key))
        {
            add_text (tuple, tfields[i].field, value);
            return;
        }
    }

    if (! strcasecmp (key, "TRACKNUMBER"))
        tuple_set_int(tuple, FIELD_TRACK_NUMBER, NULL, atoi(value));
    else if (! strcasecmp (key, "DATE"))
        tuple_set_int(tuple, FIELD_YEAR, NULL, atoi(value));
    else if (! strcasecmp (key, "REPLAYGAIN_TRACK_GAIN"))
        set_gain_info(tuple, FIELD_GAIN_TRACK_GAIN, FIELD_GAIN_GAIN_UNIT, value);
    else if (! strcasecmp (key, "REPLAYGAIN_TRACK_PEAK"))
        set_gain_info(tuple, FIELD_GAIN_TRACK_PEAK, FIELD_GAIN_PEAK_UNIT, value);
    else if (! strcasecmp (key, "REPLAYGAIN_ALBUM_GAIN"))
        set_gain_info(tuple, FIELD_GAIN_ALBUM_GAIN, FIELD_GAIN_GAIN_UNIT, value);
    else if (! strcasecmp (key, "REPLAYGAIN_ALBUM_PEAK"))
        set_gain_info(tuple, FIELD_GAIN_ALBUM_PEAK, FIELD_GAIN_PEAK_UNIT, value);
}

Tuple *flac_probe_for_tuple(const char *filename, VFSFile *fd)
{
    AUDDBG("Probe for tuple.\n");

    Tuple *tuple = NULL;
    FLAC__Metadata_Iterator *iter;
    FLAC__Metadata_Chain *chain;
    FLAC__StreamMetadata *metadata = NULL;
    FLAC__Metadata_ChainStatus status;
    FLAC__StreamMetadata_VorbisComment_Entry *entry;
    char *key;
    char *value;

    tuple = tuple_new_from_filename(filename);

    tuple_set_str(tuple, FIELD_CODEC, NULL, "Free Lossless Audio Codec (FLAC)");
    tuple_set_str(tuple, FIELD_QUALITY, NULL, _("lossless"));

    chain = FLAC__metadata_chain_new();

    if (!FLAC__metadata_chain_read_with_callbacks(chain, fd, io_callbacks))
        goto ERR;

    iter = FLAC__metadata_iterator_new();

    FLAC__metadata_iterator_init(iter, chain);

    do
    {
        switch (FLAC__metadata_iterator_get_block_type(iter))
        {
            case FLAC__METADATA_TYPE_VORBIS_COMMENT:

                if (FLAC__metadata_iterator_get_block_type(iter) == FLAC__METADATA_TYPE_VORBIS_COMMENT)
                {
                    metadata = FLAC__metadata_iterator_get_block(iter);

                    AUDDBG("Vorbis comment contains %d fields\n", metadata->data.vorbis_comment.num_comments);
                    AUDDBG("Vendor string: %s\n", metadata->data.vorbis_comment.vendor_string.entry);

                    entry = metadata->data.vorbis_comment.comments;

                    for (int i = 0; i < metadata->data.vorbis_comment.num_comments; i++, entry++)
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
                }
                break;

            case FLAC__METADATA_TYPE_STREAMINFO:
                metadata = FLAC__metadata_iterator_get_block(iter);

                /* Calculate the stream length (milliseconds) */
                if (metadata->data.stream_info.sample_rate == 0)
                {
                    FLACNG_ERROR("Invalid sample rate for stream!\n");
                    tuple_set_int(tuple, FIELD_LENGTH, NULL, -1);
                }
                else
                {
                    tuple_set_int(tuple, FIELD_LENGTH, NULL,
                        (metadata->data.stream_info.total_samples / metadata->data.stream_info.sample_rate) * 1000);
                    AUDDBG("Stream length: %d seconds\n", tuple_get_int(tuple, FIELD_LENGTH, NULL));
                }

                int64_t size = vfs_fsize(fd);

                if (size == -1 || metadata->data.stream_info.total_samples == 0)
                    tuple_set_int(tuple, FIELD_BITRATE, NULL, 0);
                else
                {
                    int bitrate = 8 * size *
                        (int64_t) metadata->data.stream_info.sample_rate / metadata->data.stream_info.total_samples;

                    tuple_set_int(tuple, FIELD_BITRATE, NULL, (bitrate + 500) / 1000);
                }
                break;

            default:
                ;
        }
    } while (FLAC__metadata_iterator_next(iter));

    FLAC__metadata_iterator_delete(iter);
    FLAC__metadata_chain_delete(chain);

    return tuple;

ERR:
    status = FLAC__metadata_chain_status(chain);
    FLAC__metadata_chain_delete(chain);

    FLACNG_ERROR("An error occured: %s\n", FLAC__Metadata_ChainStatusString[status]);
    return tuple;
}
