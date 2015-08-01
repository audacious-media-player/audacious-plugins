/*  FileWriter-Plugin
 *  (C) copyright 2007 merging of Disk Writer and Out-Lame by Michael Färber
 *
 *  Original Out-Lame-Plugin:
 *  (C) copyright 2002 Lars Siebold <khandha5@gmx.net>
 *  (C) copyright 2006-2007 porting to audacious by Yoshiki Yazawa <yaz@cc.rim.or.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <string.h>
#include <gtk/gtk.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#include "filewriter.h"
#include "convert.h"

class FileWriter : public OutputPlugin
{
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("FileWriter Plugin"),
        PACKAGE,
        about,
        & prefs
    };

    constexpr FileWriter () : OutputPlugin (info, 0, true) {}

    bool init ();

    StereoVolume get_volume () { return {0, 0}; }
    void set_volume (StereoVolume v) {}

    void set_info (const char * filename, const Tuple & tuple);
    bool open_audio (int fmt, int rate, int nch);
    void close_audio ();

    void period_wait () {}
    int write_audio (const void * ptr, int length);
    void drain () {}

    int get_delay ()
        { return 0; }

    void pause (bool pause) {}
    void flush () {}
};

EXPORT FileWriter aud_plugin_instance;

enum {
    FILENAME_ORIGINAL,
    FILENAME_ORIGINAL_NO_SUFFIX,
    FILENAME_FROM_TAG
};

/* really a boolean, but stored here as an integer
 * since WidgetRadio supports only integer settings */
static int save_original;

/* stored as two separate booleans in the config file */
static int filename_mode;

static String in_filename;
static Tuple in_tuple;

static GtkWidget * path_dirbrowser;

enum fileext_t
{
    WAV = 0,
#ifdef FILEWRITER_MP3
    MP3,
#endif
#ifdef FILEWRITER_VORBIS
    VORBIS,
#endif
#ifdef FILEWRITER_FLAC
    FLAC,
#endif
    FILEEXT_MAX
};

static const char *fileext_str[FILEEXT_MAX] =
{
    ".wav",
#ifdef FILEWRITER_MP3
    ".mp3",
#endif
#ifdef FILEWRITER_VORBIS
    ".ogg",
#endif
#ifdef FILEWRITER_FLAC
    ".flac"
#endif
};

static FileWriterImpl *plugin;
static VFSFile output_file;

FileWriterImpl *plugins[FILEEXT_MAX] = {
    &wav_plugin,
#ifdef FILEWRITER_MP3
    &mp3_plugin,
#endif
#ifdef FILEWRITER_VORBIS
    &vorbis_plugin,
#endif
#ifdef FILEWRITER_FLAC
    &flac_plugin,
#endif
};

const char * const FileWriter::defaults[] = {
#ifdef FILEWRITER_MP3
 "fileext", aud::numeric_string<MP3>::str,
#else
 "fileext", aud::numeric_string<WAV>::str,
#endif
 "filenamefromtags", "TRUE",
 "prependnumber", "FALSE",
 "save_original", "FALSE",
 "use_suffix", "FALSE",
 nullptr};

bool FileWriter::init ()
{
    aud_config_set_defaults ("filewriter", defaults);

    save_original = aud_get_bool ("filewriter", "save_original");

    if (aud_get_bool ("filewriter", "filenamefromtags"))
        filename_mode = FILENAME_FROM_TAG;
    else if (aud_get_bool ("filewriter", "use_suffix"))
        filename_mode = FILENAME_ORIGINAL;
    else
        filename_mode = FILENAME_ORIGINAL_NO_SUFFIX;

    for (auto p : plugins)
    {
        if (p->init)
            p->init ();
    }

    return true;
}

static StringBuf get_file_path ()
{
    String path = aud_get_str ("filewriter", "file_path");
    return path[0] ? str_copy (path) : filename_to_uri (g_get_home_dir ());
}

static VFSFile safe_create (const char * filename)
{
    if (! VFSFile::test_file (filename, VFS_EXISTS))
        return VFSFile (filename, "w");

    const char * extension = strrchr (filename, '.');

    for (int count = 1; count < 100; count ++)
    {
        StringBuf scratch = extension ?
         str_printf ("%.*s-%d%s", (int) (extension - filename), filename, count, extension) :
         str_printf ("%s-%d", filename, count);

        if (! VFSFile::test_file (scratch, VFS_EXISTS))
            return VFSFile (scratch, "w");
    }

    return VFSFile ();
}

void FileWriter::set_info (const char * filename, const Tuple & tuple)
{
    in_filename = String (filename);
    in_tuple = tuple.ref ();
}

static StringBuf format_filename (const char * suffix)
{
    const char * slash = in_filename ? strrchr (in_filename, '/') : nullptr;
    const char * base = slash ? slash + 1 : nullptr;

    StringBuf filename;

    if (save_original)
    {
        g_return_val_if_fail (base, StringBuf ());
        filename.insert (0, in_filename, base - in_filename);
    }
    else
    {
        filename.steal (get_file_path ());
        if (filename[filename.len () - 1] != '/')
            filename.insert (-1, "/");
    }

    if (aud_get_bool ("filewriter", "prependnumber"))
    {
        int number = in_tuple.get_int (Tuple::Track);
        if (number >= 0)
            filename.combine (str_printf ("%d%%20", number));
    }

    if (aud_get_bool ("filewriter", "filenamefromtags"))
    {
        String title = in_tuple.get_str (Tuple::FormattedTitle);

        /* truncate title at 200 bytes to avoid hitting filesystem limits */
        int len = aud::min ((int) strlen (title), 200);

        /* prevent truncation in middle of UTF-8 character */
        while ((title[len] & 0xc0) == 0x80)
            len ++;

        StringBuf buf = str_copy (title, len);

        /* replace non-portable characters */
        const char * reserved = "<>:\"/\\|?*";
        for (char * c = buf; * c; c ++)
        {
            if (strchr (reserved, * c))
                * c = ' ';
        }

        /* URI-encode */
        buf.steal (str_encode_percent (buf));
        filename.combine (std::move (buf));
    }
    else
    {
        g_return_val_if_fail (base, StringBuf ());

        const char * end = nullptr;
        if (! aud_get_bool ("filewriter", "use_suffix"))
            end = strrchr (base, '.');

        filename.insert (-1, base, end ? end - base : -1);
    }

    filename.insert (-1, suffix);
    return filename;
}

bool FileWriter::open_audio (int fmt, int rate, int nch)
{
    int ext = aud_get_int ("filewriter", "fileext");
    g_return_val_if_fail (ext >= 0 && ext < FILEEXT_MAX, false);

    StringBuf filename = format_filename (fileext_str[ext]);
    if (! filename)
        return false;

    plugin = plugins[ext];

    int out_fmt = plugin->format_required (fmt);
    convert_init (fmt, out_fmt);

    output_file = safe_create (filename);
    if (output_file && plugin->open (output_file, {out_fmt, rate, nch}, in_tuple))
        return true;

    plugin = nullptr;
    output_file = VFSFile ();
    in_filename = String ();
    in_tuple = Tuple ();
    return false;
}

int FileWriter::write_audio (const void * ptr, int length)
{
    auto & buf = convert_process (ptr, length);
    plugin->write (output_file, buf.begin (), buf.len ());

    return length;
}

void FileWriter::close_audio ()
{
    plugin->close (output_file);
    convert_free ();

    plugin = nullptr;
    output_file = VFSFile ();
    in_filename = String ();
    in_tuple = Tuple ();
}

static void * create_dirbrowser ()
{
    path_dirbrowser = gtk_file_chooser_button_new (_("Pick a folder"),
     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_uri ((GtkFileChooser *) path_dirbrowser, get_file_path ());
    gtk_widget_set_sensitive (path_dirbrowser, ! save_original);

    auto set_cb = [] ()
    {
        char * uri = gtk_file_chooser_get_uri ((GtkFileChooser *) path_dirbrowser);
        aud_set_str ("filewriter", "file_path", uri);
        g_free (uri);
    };

    // "file-set" is unreliable, so get the path on "destroy" as well
    g_signal_connect (path_dirbrowser, "file-set", set_cb, nullptr);
    g_signal_connect (path_dirbrowser, "destroy", set_cb, nullptr);

    return path_dirbrowser;
}

static void save_original_cb ()
{
    aud_set_bool ("filewriter", "save_original", save_original);
    gtk_widget_set_sensitive (path_dirbrowser, ! save_original);
}

static void filename_mode_cb ()
{
    aud_set_bool ("filewriter", "filenamefromtags", (filename_mode == FILENAME_FROM_TAG));
    aud_set_bool ("filewriter", "use_suffix", (filename_mode == FILENAME_ORIGINAL));
}

const char FileWriter::about[] =
 N_("This program is free software; you can redistribute it and/or modify "
    "it under the terms of the GNU General Public License as published by "
    "the Free Software Foundation; either version 2 of the License, or "
    "(at your option) any later version.\n\n"
    "This program is distributed in the hope that it will be useful, "
    "but WITHOUT ANY WARRANTY; without even the implied warranty of "
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
    "GNU General Public License for more details.\n\n"
    "You should have received a copy of the GNU General Public License "
    "along with this program; if not, write to the Free Software "
    "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, "
    "USA.");

static const ComboItem plugin_combo[] = {
    ComboItem ("WAV", WAV)
#ifdef FILEWRITER_MP3
    ,ComboItem ("MP3", MP3)
#endif
#ifdef FILEWRITER_VORBIS
    ,ComboItem ("Vorbis", VORBIS)
#endif
#ifdef FILEWRITER_FLAC
    ,ComboItem ("FLAC", FLAC)
#endif
};

static const PreferencesWidget main_widgets[] = {
    WidgetCombo (N_("Output file format:"),
        WidgetInt ("filewriter", "fileext"),
        {{plugin_combo}}),
    WidgetSeparator ({true}),
    WidgetRadio (N_("Save into original directory"),
        WidgetInt (save_original, save_original_cb),
        {true}),
    WidgetRadio (N_("Save into custom directory:"),
        WidgetInt (save_original, save_original_cb),
        {false}),
    WidgetCustomGTK (create_dirbrowser),
    WidgetSeparator ({true}),
    WidgetLabel (N_("Generate file name from:")),
    WidgetRadio (N_("Original file name"),
        WidgetInt (filename_mode, filename_mode_cb),
        {FILENAME_ORIGINAL}),
    WidgetRadio (N_("Original file name (no suffix)"),
        WidgetInt (filename_mode, filename_mode_cb),
        {FILENAME_ORIGINAL_NO_SUFFIX}),
    WidgetRadio (N_("Original file tag"),
        WidgetInt (filename_mode, filename_mode_cb),
        {FILENAME_FROM_TAG}),
    WidgetSeparator ({true}),
    WidgetCheck (N_("Prepend track number to file name"),
        WidgetBool ("filewriter", "prependnumber"))
};

#ifdef FILEWRITER_MP3
static const PreferencesWidget mp3_widgets[] = {
    WidgetCustomGTK (mp3_configure)
};
#endif

#ifdef FILEWRITER_VORBIS
static const PreferencesWidget vorbis_widgets[] = {
    WidgetCustomGTK (vorbis_configure)
};
#endif

static const NotebookTab tabs[] = {
    {N_("General"), {main_widgets}}
#ifdef FILEWRITER_MP3
    ,{"MP3", {mp3_widgets}}
#endif
#ifdef FILEWRITER_VORBIS
    ,{"Vorbis", {vorbis_widgets}}
#endif
};

const PreferencesWidget FileWriter::widgets[] = {
    WidgetNotebook ({{tabs}})
};

const PluginPreferences FileWriter::prefs = {{widgets}};
