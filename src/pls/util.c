/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team
 *
 *  Based on BMP:
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include <stdlib.h>
#include <string.h>

#include <libaudcore/vfs.h>

#include "util.h"

static void strip_string(GString *string)
{
    while (string->len > 0 && string->str[0] == ' ')
        g_string_erase(string, 0, 1);

    while (string->len > 0 && string->str[string->len - 1] == ' ')
        g_string_erase(string, string->len - 1, 1);
}

static void strip_lower_string(GString *string)
{
    gchar *lower;
    strip_string(string);

    lower = g_ascii_strdown(string->str, -1);
    g_free(string->str);
    string->str = lower;
}

static void close_ini_file_free_value(gpointer value)
{
    g_free((gchar *)value);
}

static void close_ini_file_free_section(gpointer section)
{
    g_hash_table_destroy((GHashTable *)section);
}

INIFile * open_ini_file (VFSFile * file)
{
    GHashTable *ini_file = NULL;
    GHashTable *section = NULL;
    GString *section_name, *key_name, *value;
    gpointer section_hash, key_hash;
    gsize off = 0;

    gchar * buffer = NULL;
    gint64 filesize = vfs_fsize (file);

    if (filesize > 0)
    {
        buffer = g_malloc (filesize);
        filesize = vfs_fread (buffer, 1, filesize, file);
    }
    else
    {
        gint64 readed;
        filesize = 0;

        do
        {
            buffer = g_realloc (buffer, filesize + 4096);
            readed = vfs_fread (buffer + filesize, 1, 4096, file);
            filesize += readed;
        }
        while (readed > 0);
    }

    if (filesize < 1)
    {
        g_free (buffer);
        return NULL;
    }

    section_name = g_string_new("");
    key_name = g_string_new(NULL);
    value = g_string_new(NULL);

    ini_file =
        g_hash_table_new_full(NULL, NULL, NULL, close_ini_file_free_section);
    section =
        g_hash_table_new_full(NULL, NULL, NULL, close_ini_file_free_value);
    /* make a nameless section which should store all entries that are not
     * embedded in a section */
    section_hash = GINT_TO_POINTER(g_string_hash(section_name));
    g_hash_table_insert(ini_file, section_hash, section);

    while (off < filesize)
    {
        /* ignore the following characters */
        if (buffer[off] == '\r' || buffer[off] == '\n' || buffer[off] == ' '
            || buffer[off] == '\t')
        {
            if (buffer[off] == '\n')
            {
                g_string_free(key_name, TRUE);
                g_string_free(value, TRUE);
                key_name = g_string_new(NULL);
                value = g_string_new(NULL);
            }

            off++;
            continue;
        }

        /* if we encounter a possible section statement */
        if (buffer[off] == '[')
        {
            g_string_free(section_name, TRUE);
            section_name = g_string_new(NULL);
            off++;

            if (off >= filesize)
                goto return_sequence;

            while (buffer[off] != ']')
            {
                /* if the section statement has not been closed before a
                 * linebreak */
                if (buffer[off] == '\n')
                    break;

                g_string_append_c(section_name, buffer[off]);
                off++;
                if (off >= filesize)
                    goto return_sequence;
            }
            if (buffer[off] == '\n')
                continue;
            if (buffer[off] == ']')
            {
                off++;
                if (off >= filesize)
                    goto return_sequence;

                strip_lower_string(section_name);
                section_hash = GINT_TO_POINTER(g_string_hash(section_name));

                /* if this section already exists, we don't make a new one,
                 * but reuse the old one */
                if (g_hash_table_lookup(ini_file, section_hash) != NULL)
                    section = g_hash_table_lookup(ini_file, section_hash);
                else
                {
                    section =
                        g_hash_table_new_full(NULL, NULL, NULL,
                                              close_ini_file_free_value);
                    g_hash_table_insert(ini_file, section_hash, section);
                }

                continue;
            }
        }

        if (buffer[off] == '=')
        {
            off++;
            if (off >= filesize)
                goto return_sequence;

            while (buffer[off] != '\n' && buffer[off] != '\r')
            {
                g_string_append_c(value, buffer[off]);
                off++;
                if (off >= filesize)
                    break;
            }

            strip_lower_string(key_name);
            key_hash = GINT_TO_POINTER(g_string_hash(key_name));
            strip_string(value);

            if (key_name->len > 0 && value->len > 0)
                g_hash_table_insert(section, key_hash, g_strdup(value->str));
        }
        else
        {
            g_string_append_c(key_name, buffer[off]);
            off++;
            if (off >= filesize)
                goto return_sequence;
        }
    }

  return_sequence:
    g_string_free(section_name, TRUE);
    g_string_free(key_name, TRUE);
    g_string_free(value, TRUE);
    g_free(buffer);
    return ini_file;
}

/**
 * Frees the memory allocated for inifile.
 */
void close_ini_file(INIFile *inifile)
{
    g_return_if_fail(inifile);
    g_hash_table_destroy(inifile);
}

/**
 * Returns a string that corresponds to correct section and key in inifile.
 *
 * Returns NULL if value was not found in inifile. Otherwise returns a copy
 * of string pointed by "section" and "key". Returned string should be freed
 * after use.
 */
gchar *read_ini_string(INIFile *inifile, const gchar *section, const gchar *key)
{
    GString *section_string;
    GString *key_string;
    gchar *value = NULL;
    gpointer section_hash, key_hash;
    GHashTable *section_table;

    g_return_val_if_fail(inifile, NULL);

    section_string = g_string_new(section);
    key_string = g_string_new(key);
    value = NULL;

    strip_lower_string(section_string);
    strip_lower_string(key_string);
    section_hash = GINT_TO_POINTER(g_string_hash(section_string));
    key_hash = GINT_TO_POINTER(g_string_hash(key_string));
    section_table = g_hash_table_lookup(inifile, section_hash);

    if (section_table)
    {
        value =
            g_strdup(g_hash_table_lookup
                     (section_table, GINT_TO_POINTER(key_hash)));
    }

    g_string_free(section_string, TRUE);
    g_string_free(key_string, TRUE);

    g_return_val_if_fail(value, NULL);
    return value;
}
