/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2009  Audacious development team
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

#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include <audacious/debug.h>
#include <audacious/i18n.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/vfs.h>

#include "ui_main.h"
#include "util.h"

typedef struct
{
    const gchar *to_match;
    gchar *match;
    gboolean found;
} FindFileContext;

#ifndef HAVE_MKDTEMP
static void make_directory(const gchar *path, mode_t mode);
#endif

gchar * find_file_case (const gchar * folder, const gchar * basename)
{
    static GHashTable * cache = NULL;
    GList * list = NULL;
    void * vlist;

    if (cache == NULL)
        cache = g_hash_table_new (g_str_hash, g_str_equal);

    if (g_hash_table_lookup_extended (cache, folder, NULL, & vlist))
        list = vlist;
    else
    {
        DIR * handle;
        struct dirent * entry;

        if ((handle = opendir (folder)) == NULL)
            return NULL;

        while ((entry = readdir (handle)) != NULL)
            list = g_list_prepend (list, g_strdup (entry->d_name));

        g_hash_table_insert (cache, g_strdup (folder), list);
        closedir (handle);
    }

    for (; list != NULL; list = list->next)
    {
        if (! strcasecmp (list->data, basename))
            return g_strdup (list->data);
    }

    return NULL;
}

gchar * find_file_case_path (const gchar * folder, const gchar * basename)
{
    gchar * found, * path;

    if ((found = find_file_case (folder, basename)) == NULL)
        return NULL;

    path = g_strdup_printf ("%s/%s", folder, found);
    g_free (found);
    return path;
}

gchar * find_file_case_uri (const gchar * folder, const gchar * basename)
{
    gchar * found, * uri;

    if ((found = find_file_case_path (folder, basename)) == NULL)
        return NULL;

    uri = g_filename_to_uri (found, NULL, NULL);
    g_free (found);
    return uri;
}

gchar * load_text_file (const gchar * filename)
{
    VFSFile * file;
    gint size;
    gchar * buffer;

    file = vfs_fopen (filename, "r");

    if (file == NULL)
        return NULL;

    size = vfs_fsize (file);
    size = MAX (0, size);
    buffer = g_malloc (size + 1);

    size = vfs_fread (buffer, 1, size, file);
    size = MAX (0, size);
    buffer[size] = 0;

    vfs_fclose (file);

    return buffer;
}

gchar * text_parse_line (gchar * text)
{
    gchar * newline = strchr (text, '\n');

    if (newline == NULL)
        return NULL;

    * newline = 0;
    return newline + 1;
}

typedef enum
{
    ARCHIVE_UNKNOWN = 0,
    ARCHIVE_DIR,
    ARCHIVE_TAR,
    ARCHIVE_TGZ,
    ARCHIVE_ZIP,
    ARCHIVE_TBZ2
} ArchiveType;

typedef gchar *(*ArchiveExtractFunc) (const gchar *, const gchar *);

typedef struct
{
    ArchiveType type;
    const gchar *ext;
} ArchiveExtensionType;

static ArchiveExtensionType archive_extensions[] = {
    {ARCHIVE_TAR, ".tar"},
    {ARCHIVE_ZIP, ".wsz"},
    {ARCHIVE_ZIP, ".zip"},
    {ARCHIVE_TGZ, ".tar.gz"},
    {ARCHIVE_TGZ, ".tgz"},
    {ARCHIVE_TBZ2, ".tar.bz2"},
    {ARCHIVE_TBZ2, ".bz2"},
    {ARCHIVE_UNKNOWN, NULL}
};

static gchar *archive_extract_tar(const gchar *archive, const gchar *dest);
static gchar *archive_extract_zip(const gchar *archive, const gchar *dest);
static gchar *archive_extract_tgz(const gchar *archive, const gchar *dest);
static gchar *archive_extract_tbz2(const gchar *archive, const gchar *dest);

static ArchiveExtractFunc archive_extract_funcs[] = {
    NULL,
    NULL,
    archive_extract_tar,
    archive_extract_tgz,
    archive_extract_zip,
    archive_extract_tbz2
};


/* FIXME: these functions can be generalised into a function using a
 * command lookup table */

static const gchar *get_tar_command(void)
{
    static const gchar *command = NULL;

    if (!command)
    {
        if (!(command = getenv("TARCMD")))
            command = "tar";
    }

    return command;
}

static const gchar *get_unzip_command(void)
{
    static const gchar *command = NULL;

    if (!command)
    {
        if (!(command = getenv("UNZIPCMD")))
            command = "unzip";
    }

    return command;
}


static gchar *archive_extract_tar(const gchar *archive, const gchar *dest)
{
    return g_strdup_printf("%s >/dev/null xf \"%s\" -C %s", get_tar_command(),
                           archive, dest);
}

static gchar *archive_extract_zip(const gchar *archive, const gchar *dest)
{
    return g_strdup_printf("%s >/dev/null -o -j \"%s\" -d %s",
                           get_unzip_command(), archive, dest);
}

static gchar *archive_extract_tgz(const gchar *archive, const gchar *dest)
{
    return g_strdup_printf("%s >/dev/null xzf \"%s\" -C %s", get_tar_command(),
                           archive, dest);
}

static gchar *archive_extract_tbz2(const gchar *archive, const gchar *dest)
{
    return g_strdup_printf("bzip2 -dc \"%s\" | %s >/dev/null xf - -C %s",
                           archive, get_tar_command(), dest);
}


static ArchiveType archive_get_type(const gchar *filename)
{
    gint i = 0;

    if (g_file_test(filename, G_FILE_TEST_IS_DIR))
        return ARCHIVE_DIR;

    while (archive_extensions[i].ext)
    {
        if (g_str_has_suffix(filename, archive_extensions[i].ext))
        {
            return archive_extensions[i].type;
        }
        i++;
    }

    return ARCHIVE_UNKNOWN;
}

gboolean file_is_archive(const gchar *filename)
{
    return (archive_get_type(filename) > ARCHIVE_DIR);
}

gchar *archive_basename(const gchar *str)
{
    gint i = 0;

    while (archive_extensions[i].ext)
    {
        if (str_has_suffix_nocase(str, archive_extensions[i].ext))
        {
            const gchar *end = g_strrstr(str, archive_extensions[i].ext);
            if (end)
            {
                return g_strndup(str, end - str);
            }
            break;
        }
        i++;
    }

    return NULL;
}

/**
 * Escapes characters that are special to the shell inside double quotes.
 *
 * @param string String to be escaped.
 * @return Given string with special characters escaped. Must be freed with g_free().
 */
static gchar *
escape_shell_chars(const gchar * string)
{
    const gchar *special = "$`\"\\";    /* Characters to escape */
    const gchar *in = string;
    gchar *out, *escaped;
    gint num = 0;

    while (*in != '\0')
        if (strchr(special, *in++))
            num++;

    escaped = g_malloc(strlen(string) + num + 1);

    in = string;
    out = escaped;

    while (*in != '\0') {
        if (strchr(special, *in))
            *out++ = '\\';
        *out++ = *in++;
    }
    *out = '\0';

    return escaped;
}

/*
   decompress_archive

   Decompresses the archive "filename" to a temporary directory,
   returns the path to the temp dir, or NULL if failed,
   watch out tho, doesn't actually check if the system command succeeds :-|
*/

gchar *archive_decompress(const gchar *filename)
{
    gchar *tmpdir, *cmd, *escaped_filename;
    ArchiveType type;
#ifndef HAVE_MKDTEMP
#ifdef S_IRGRP
    mode_t mode755 = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
#else
    mode_t mode755 = S_IRWXU;
#endif
#endif

    if ((type = archive_get_type(filename)) <= ARCHIVE_DIR)
        return NULL;

#ifdef HAVE_MKDTEMP
    tmpdir = g_build_filename(g_get_tmp_dir(), "audacious.XXXXXXXX", NULL);
    if (!mkdtemp(tmpdir))
    {
        g_free(tmpdir);
        AUDDBG("Unable to load skin: Failed to create temporary "
               "directory: %s\n", g_strerror(errno));
        return NULL;
    }
#else
    tmpdir = g_strdup_printf("%s/audacious.%ld", g_get_tmp_dir(), (long)rand());
    make_directory(tmpdir, mode755);
#endif

    escaped_filename = escape_shell_chars(filename);
    cmd = archive_extract_funcs[type] (escaped_filename, tmpdir);
    g_free(escaped_filename);

    if (!cmd)
    {
        AUDDBG("extraction function is NULL!\n");
        g_free(tmpdir);
        return NULL;
    }

    AUDDBG("Attempt to execute \"%s\"\n", cmd);

    if (system(cmd) != 0)
    {
        AUDDBG("could not execute cmd %s\n", cmd);
        g_free(cmd);
        return NULL;
    }
    g_free(cmd);

    return tmpdir;
}

static gboolean del_directory_func(const gchar *path, const gchar *basename,
                                   void *params)
{
    if (!strcmp(basename, ".") || !strcmp(path, ".."))
        return FALSE;

    if (g_file_test(path, G_FILE_TEST_IS_DIR))
    {
        dir_foreach(path, del_directory_func, NULL, NULL);
        rmdir(path);
        return FALSE;
    }

    unlink(path);

    return FALSE;
}

void del_directory(const gchar *path)
{
    dir_foreach(path, del_directory_func, NULL, NULL);
    rmdir(path);
}

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

INIFile *open_ini_file(const gchar *filename)
{
    GHashTable *ini_file = NULL;
    GHashTable *section = NULL;
    GString *section_name, *key_name, *value;
    gpointer section_hash, key_hash;
    guchar * buffer = NULL;
    gsize off = 0;
    gint64 filesize = 0;

    unsigned char x[] = { 0xff, 0xfe, 0x00 };

    g_return_val_if_fail(filename, NULL);

    void * vbuf = NULL;
    vfs_file_get_contents (filename, & vbuf, & filesize);
    if (! vbuf)
        return NULL;
    buffer = vbuf;

    /*
     * Convert UTF-16 into something useful. Original implementation
     * by incomp@#audacious. Cleanups \nenolod
     * FIXME: can't we use a GLib function for that? -- 01mf02
     */
    if (filesize > 2 && !memcmp(&buffer[0], &x, 2))
    {
        guchar *outbuf = g_malloc (filesize); /* it's safe to waste memory. */
        guint counter;

        for (counter = 2; counter < filesize; counter += 2)
        {
            if (!memcmp(&buffer[counter + 1], &x[2], 1))
            {
                outbuf[(counter - 2) / 2] = buffer[counter];
            }
            else
            {
                g_free(buffer);
                g_free(outbuf);
                return NULL;
            }
        }

        outbuf[(counter - 2) / 2] = '\0';

        if ((filesize - 2) / 2 == (counter - 2) / 2)
        {
            g_free(buffer);
            buffer = outbuf;
        }
        else
        {
            g_free(buffer);
            g_free(outbuf);
            return NULL;        /* XXX wrong encoding */
        }
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

    return value;
}

GArray *read_ini_array(INIFile *inifile, const gchar *section, const gchar *key)
{
    gchar *temp;
    GArray *a;

    if (! (temp = read_ini_string (inifile, section, key)))
        return NULL;

    a = string_to_garray(temp);
    g_free(temp);
    return a;
}

GArray *string_to_garray(const gchar *str)
{
    GArray *array;
    gint temp;
    const gchar *ptr = str;
    gchar *endptr;

    array = g_array_new(FALSE, TRUE, sizeof(gint));
    for (;;)
    {
        temp = strtol(ptr, &endptr, 10);
        if (ptr == endptr)
            break;
        g_array_append_val(array, temp);
        ptr = endptr;
        while (!isdigit((int)*ptr) && (*ptr) != '\0')
            ptr++;
        if (*ptr == '\0')
            break;
    }
    return (array);
}

gboolean dir_foreach(const gchar *path, DirForeachFunc function,
                     gpointer user_data, GError **error)
{
    GError *error_out = NULL;
    GDir *dir;
    const gchar *entry;
    gchar *entry_fullpath;

    if (!(dir = g_dir_open(path, 0, &error_out)))
    {
        g_propagate_error(error, error_out);
        return FALSE;
    }

    while ((entry = g_dir_read_name(dir)))
    {
        entry_fullpath = g_build_filename(path, entry, NULL);

        if ((*function) (entry_fullpath, entry, user_data))
        {
            g_free(entry_fullpath);
            break;
        }

        g_free(entry_fullpath);
    }

    g_dir_close(dir);

    return TRUE;
}

GtkWidget *make_filebrowser(const gchar *title, gboolean save)
{
    GtkWidget *dialog;
    GtkWidget *button;

    g_return_val_if_fail(title != NULL, NULL);

    dialog =
        gtk_file_chooser_dialog_new(title, GTK_WINDOW(mainwin),
                                    save ? GTK_FILE_CHOOSER_ACTION_SAVE :
                                    GTK_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);

    button =
        gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL,
                              GTK_RESPONSE_REJECT);

    gtk_button_set_use_stock(GTK_BUTTON(button), TRUE);
    gtk_widget_set_can_default (button, TRUE);

    button =
        gtk_dialog_add_button(GTK_DIALOG(dialog),
                              save ? GTK_STOCK_SAVE : GTK_STOCK_OPEN,
                              GTK_RESPONSE_ACCEPT);

    gtk_button_set_use_stock(GTK_BUTTON(button), TRUE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);    /* centering */

    return dialog;
}

#ifndef HAVE_MKDTEMP
static void make_directory(const gchar *path, mode_t mode)
{
    if (g_mkdir_with_parents(path, mode) == 0)
        return;

    g_printerr(_("Could not create directory (%s): %s\n"), path,
               g_strerror(errno));
}
#endif

void check_set (GtkActionGroup * action_group, const gchar * action_name,
 gboolean is_on)
{
    GtkAction * action = gtk_action_group_get_action (action_group, action_name);

    g_return_if_fail (action != NULL);

    gtk_toggle_action_set_active ((GtkToggleAction *) action, is_on);
}
