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

#include <string.h>
#include <audacious/debug.h>

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
        FLACNG_ERROR("Could not seek to %lld!\n", (long long)offset);
        return -1;
    }

    return 0;
}

static FLAC__int64 tell_cb(FLAC__IOHandle handle)
{
    guint64 offset;

    if ((offset = vfs_ftell(handle)) == -1)
    {
        FLACNG_ERROR("Could not tell current position!\n");
        return -1;
    }

    AUDDBG ("Current position: %d\n", (gint) offset);
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
 const Tuple * tuple, gint tuple_name, gchar * field_name)
{
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    gchar *str;
    gchar *val = tuple_get_str(tuple, tuple_name, NULL);

    if (val == NULL)
        return;

    str  = g_strdup_printf("%s=%s", field_name, val);
    entry.entry = (FLAC__byte *) str;
    entry.length = strlen(str);
    FLAC__metadata_object_vorbiscomment_insert_comment(vc_block,
        vc_block->data.vorbis_comment.num_comments, entry, true);
    g_free(str);
    str_unref(val);
}

static void insert_int_tuple_to_vc (FLAC__StreamMetadata * vc_block,
 const Tuple * tuple, gint tuple_name, gchar * field_name)
{
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    gchar *str;
    gint val = tuple_get_int(tuple, tuple_name, NULL);

    if (val <= 0)
        return;

    if (FIELD_TRACK_NUMBER == tuple_name)
        str  = g_strdup_printf("%s=%.2d", field_name, val);
    else
        str  = g_strdup_printf("%s=%d", field_name, val);

    entry.entry = (FLAC__byte *) str;
    entry.length = strlen(str);
    FLAC__metadata_object_vorbiscomment_insert_comment(vc_block,
        vc_block->data.vorbis_comment.num_comments, entry, true);
    g_free(str);
}

gboolean flac_update_song_tuple(const Tuple *tuple, VFSFile *fd)
{
    AUDDBG("Update song tuple.\n");

    FLAC__Metadata_Iterator *iter;
    FLAC__Metadata_Chain *chain;
    FLAC__StreamMetadata *vc_block;
    FLAC__Metadata_ChainStatus status;

    chain = FLAC__metadata_chain_new();
    g_return_val_if_fail(chain != NULL, FALSE);

    if (!FLAC__metadata_chain_read_with_callbacks(chain, fd, io_callbacks))
        goto ERR;

    iter = FLAC__metadata_iterator_new();
    g_return_val_if_fail(iter != NULL, FALSE);

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

gboolean flac_get_image(const gchar *filename, VFSFile *fd, void **data, gint64 *length)
{
    AUDDBG("Probe for song image.\n");

    FLAC__Metadata_Iterator *iter;
    FLAC__Metadata_Chain *chain;
    FLAC__StreamMetadata *metadata = NULL;
    FLAC__Metadata_ChainStatus status;
    gboolean has_image = FALSE;

    chain = FLAC__metadata_chain_new();
    g_return_val_if_fail(chain != NULL, FALSE);

    if (!FLAC__metadata_chain_read_with_callbacks(chain, fd, io_callbacks))
        goto ERR;

    iter = FLAC__metadata_iterator_new();
    g_return_val_if_fail(iter != NULL, FALSE);

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

            *data = g_memdup(metadata->data.picture.data, metadata->data.picture.data_length);
            *length = metadata->data.picture.data_length;
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

static void parse_gain_text(const gchar *text, gint *value, gint *unit)
{
    gint sign = 1;

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

        while (*text >= '0' && *text <= '9' && *value < G_MAXINT / 10)
        {
            *value = *value * 10 + (*text - '0');
            *unit = *unit * 10;
            text++;
        }
    }

    *value = *value * sign;
}

static void set_gain_info(Tuple *tuple, gint field, gint unit_field, const gchar *text)
{
    gint value, unit;

    parse_gain_text(text, &value, &unit);

    if (tuple_get_value_type(tuple, unit_field, NULL) == TUPLE_INT)
        value = value * (gint64) tuple_get_int(tuple, unit_field, NULL) / unit;
    else
        tuple_set_int(tuple, unit_field, NULL, unit);

    tuple_set_int(tuple, field, NULL, value);
}

static void add_text (Tuple * tuple, gint field, const gchar * value)
{
    gchar * cur = tuple_get_str (tuple, field, NULL);
    if (cur)
    {
        gchar * both = g_strconcat (cur, ", ", value, NULL);
        tuple_set_str (tuple, field, NULL, both);
        g_free(both);
    }
    else
        tuple_set_str (tuple, field, NULL, value);

    str_unref(cur);
}

static void parse_comment (Tuple * tuple, const gchar * key, const gchar * value)
{
    AUDDBG ("Found key %s <%s>\n", key, value);

    const struct {
        const gchar * key;
        int field;
    } tfields[] = {
     {"ARTIST", FIELD_ARTIST},
     {"ALBUM", FIELD_ALBUM},
     {"TITLE", FIELD_TITLE},
     {"COMMENT", FIELD_COMMENT},
     {"GENRE", FIELD_GENRE}};

    for (gint i = 0; i < G_N_ELEMENTS (tfields); i ++)
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

Tuple *flac_probe_for_tuple(const gchar *filename, VFSFile *fd)
{
    AUDDBG("Probe for tuple.\n");

    Tuple *tuple = NULL;
    FLAC__Metadata_Iterator *iter;
    FLAC__Metadata_Chain *chain;
    FLAC__StreamMetadata *metadata = NULL;
    FLAC__Metadata_ChainStatus status;
    FLAC__StreamMetadata_VorbisComment_Entry *entry;
    gchar *key;
    gchar *value;

    tuple = tuple_new_from_filename(filename);

    tuple_set_str(tuple, FIELD_CODEC, NULL, "Free Lossless Audio Codec (FLAC)");
    tuple_set_str(tuple, FIELD_QUALITY, NULL, "lossless");

    chain = FLAC__metadata_chain_new();
    g_return_val_if_fail(chain != NULL, FALSE);

    if (!FLAC__metadata_chain_read_with_callbacks(chain, fd, io_callbacks))
        goto ERR;

    iter = FLAC__metadata_iterator_new();
    g_return_val_if_fail(iter != NULL, FALSE);

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

                    for (gint i = 0; i < metadata->data.vorbis_comment.num_comments; i++, entry++)
                    {
                        if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(*entry, &key, &value) == false)
                            AUDDBG("Could not parse comment\n");
                        else
                        {
                            parse_comment(tuple, key, value);
                            g_free(key);
                            g_free(value);
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

                gsize size = vfs_fsize(fd);

                if (size == -1 || metadata->data.stream_info.total_samples == 0)
                    tuple_set_int(tuple, FIELD_BITRATE, NULL, 0);
                else
                {
                    gint bitrate = 8 * size *
                        (gint64) metadata->data.stream_info.sample_rate / metadata->data.stream_info.total_samples;

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
