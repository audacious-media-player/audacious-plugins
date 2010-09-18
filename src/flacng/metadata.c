/*
 * A FLAC decoder plugin for the Audacious Media Player
 * Copyright 2010 Michał Lipski <tallica@o2.pl>
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

#include <audacious/debug.h>

#include "flacng.h"

static size_t read_cb(void *ptr, size_t size, size_t nmemb, FLAC__IOHandle handle)
{
    AUDDBG("Read callback.\n");
    size_t read;

    if (handle == NULL)
    {
        ERROR("Trying to read data from an uninitialized file!\n");
        return -1;
    }

    read = vfs_fread(ptr, size, nmemb, handle);

    switch (read)
    {
        case -1:
            ERROR("Error while reading from stream!\n");
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
    AUDDBG("Write callback.\n");

    return vfs_fwrite(ptr, size, nmemb, handle);
}

static int seek_cb(FLAC__IOHandle handle, FLAC__int64 offset, int whence)
{
    AUDDBG("Seek callback.\n");

    if (vfs_fseek(handle, offset, whence) != 0)
    {
        ERROR("Could not seek to %lld!\n", (long long)offset);
        return -1;
    }

    return 0;
}

static FLAC__int64 tell_cb(FLAC__IOHandle handle)
{
    guint64 offset;

    if ((offset = vfs_ftell(handle)) == -1)
    {
        ERROR("Could not tell current position!\n");
        return -1;
    }

    AUDDBG("Current position: %ld\n", offset);
    return offset;
}

static int eof_cb(FLAC__IOHandle handle)
{
    AUDDBG("EOF callback\n");

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
    gchar *val = (gchar *) tuple_get_string(tuple, tuple_name, NULL);

    if (val == NULL)
        return;

    str  = g_strdup_printf("%s=%s", field_name, val);
    entry.entry = (FLAC__byte *) str;
    entry.length = strlen(str);
    FLAC__metadata_object_vorbiscomment_insert_comment(vc_block,
        vc_block->data.vorbis_comment.num_comments, entry, true);
    g_free(str);
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
        goto ERROR;

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
        goto ERROR;

    FLAC__metadata_chain_delete(chain);
    return TRUE;

ERROR:
    status = FLAC__metadata_chain_status(chain);
    FLAC__metadata_chain_delete(chain);

    ERROR("An error occured: %s\n", FLAC__Metadata_ChainStatusString[status]);
    return FALSE;
}

gboolean flac_get_image(const gchar *filename, VFSFile *fd, void **data, gint *length)
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
        goto ERROR;

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
            *length = (guint32) metadata->data.picture.data_length;
            has_image = TRUE;
        }
    }

    FLAC__metadata_chain_delete(chain);
    return has_image;

ERROR:
    status = FLAC__metadata_chain_status(chain);
    FLAC__metadata_chain_delete(chain);

    ERROR("An error occured: %s\n", FLAC__Metadata_ChainStatusString[status]);
    return FALSE;
}
