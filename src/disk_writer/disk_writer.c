/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2004  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2004  Haavard Kvaalen
 *
 *  File name suffix option added by Heikki Orsila 2003
 *  <heikki.orsila@iki.fi> (no copyrights claimed)
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

#include "config.h"

#include <gtk/gtk.h>
#include <audacious/i18n.h>
#include <stdio.h>
#include <string.h>

#include "audacious/plugin.h"
#include "audacious/beepctrl.h"
#include "audacious/configdb.h"
#include "audacious/util.h"
#include "audacious/vfs.h"


struct format_info { 
    AFormat format;
    int frequency;
    int channels;
};
struct format_info input;

struct wavhead
{
    guint32 main_chunk;
    guint32 length;
    guint32 chunk_type;
    guint32 sub_chunk;
    guint32 sc_len;
    guint16 format;
    guint16 modus;
    guint32 sample_fq;
    guint32 byte_p_sec;
    guint16 byte_p_spl;
    guint16 bit_p_spl;
    guint32 data_chunk;
    guint32 data_length;
};

static GtkWidget *configure_win = NULL, *configure_vbox;
static GtkWidget *path_hbox, *path_label, *path_dirbrowser;
static GtkWidget *configure_separator;
static GtkWidget *configure_bbox, *configure_ok, *configure_cancel;

static GtkWidget *saveplace_hbox, *saveplace;
static gboolean save_original = TRUE;

static GtkWidget *use_suffix_toggle = NULL;
static gboolean use_suffix = FALSE;

static gchar *file_path = NULL;
static VFSFile *output_file = NULL;
static struct wavhead header;
static guint64 written = 0;
static AFormat afmt;
static gint arate, ach;
gint ctrlsocket_get_session_id(void);       /* FIXME */

static void disk_init(void);
static gint disk_open(AFormat fmt, gint rate, gint nch);
static void disk_write(void *ptr, gint length);
static void disk_close(void);
static void disk_flush(gint time);
static void disk_pause(short p);
static gint disk_free(void);
static gint disk_playing(void);
static gint disk_get_written_time(void);
static gint disk_get_output_time(void);
static void disk_configure(void);
static void disk_getvol(gint *, gint *);
static void disk_setvol(gint, gint);

static int lvol = 0, rvol = 0;

OutputPlugin disk_op =
{
    NULL,
    NULL,
    NULL,
    disk_init,
    NULL,
    NULL,
    disk_configure,
    disk_getvol,
    disk_setvol,
    disk_open,
    disk_write,
    disk_close,
    disk_flush,
    disk_pause,
    disk_free,
    disk_playing,
    disk_get_output_time,
    disk_get_written_time,
    NULL
};

OutputPlugin *get_oplugin_info(void)
{
    disk_op.description = g_strdup_printf(_("Disk Writer Plugin %s"), VERSION);
    return &disk_op;
}

static void disk_init(void)
{
    ConfigDb *db;

    db = bmp_cfg_db_open();
    bmp_cfg_db_get_string(db, "disk_writer", "file_path", &file_path);
    bmp_cfg_db_get_bool(db, "disk_writer", "save_original", &save_original);
    bmp_cfg_db_get_bool(db, "disk_writer", "use_suffix", &use_suffix);
    bmp_cfg_db_close(db);

    if (!file_path)
        file_path = g_strdup(g_get_home_dir());
}

static gint disk_open(AFormat fmt, gint rate, gint nch)
{
    gchar *filename, *title, *temp;
    gint pos;

    written = 0;
    afmt = fmt;
    arate = rate;
    ach = nch;

    if (xmms_check_realtime_priority())
    {
        xmms_show_message(_("Error"),
                          _("You cannot use the Disk Writer plugin\n"
                            "when you're running in realtime mode."),
                          _("OK"), FALSE, NULL, NULL);
        return 0;
    }

    pos = xmms_remote_get_playlist_pos(ctrlsocket_get_session_id());
    title = xmms_remote_get_playlist_file(ctrlsocket_get_session_id(), pos);
    if (!use_suffix) {
        if (title != NULL && (temp = strrchr(title, '.')) != NULL) {
            *temp = '\0';
        }
    }
    if (title == NULL || strlen(g_basename(title)) == 0)
    {
        g_free(title);
        /* No filename, lets try title instead */
        title = xmms_remote_get_playlist_title(ctrlsocket_get_session_id(), pos);
        while (title != NULL && (temp = strchr(title, '/')) != NULL)
            *temp = '-';

        if (title == NULL || strlen(g_basename(title)) == 0)
        {
            g_free(title);
            /* No title either.  Just set it to something. */
            title = g_strdup_printf("xmms-%d", pos);
        }
    }

    if (save_original)
        filename = g_strdup_printf("%s.wav", title);
    else
        filename = g_strdup_printf("%s/%s.wav", file_path, g_basename(title));
    // FIXME: check if filename equals title
    g_free(title);

    output_file = vfs_fopen(filename, "wb");
    g_free(filename);

    if (!output_file)
        return 0;

    memcpy(&header.main_chunk, "RIFF", 4);
    header.length = GUINT32_TO_LE(0);
    memcpy(&header.chunk_type, "WAVE", 4);
    memcpy(&header.sub_chunk, "fmt ", 4);
    header.sc_len = GUINT32_TO_LE(16);
    header.format = GUINT16_TO_LE(1);
    header.modus = GUINT16_TO_LE(nch);
    header.sample_fq = GUINT32_TO_LE(rate);
    if (fmt == FMT_U8 || fmt == FMT_S8)
        header.bit_p_spl = GUINT16_TO_LE(8);
    else
        header.bit_p_spl = GUINT16_TO_LE(16);
    header.byte_p_sec = GUINT32_TO_LE(rate * header.modus * (GUINT16_FROM_LE(header.bit_p_spl) / 8));
    header.byte_p_spl = GUINT16_TO_LE((GUINT16_FROM_LE(header.bit_p_spl) / (8 / nch)));
    memcpy(&header.data_chunk, "data", 4);
    header.data_length = GUINT32_TO_LE(0);
    vfs_fwrite(&header, sizeof (struct wavhead), 1, output_file);

    input.format = fmt;
    input.frequency = rate;
    input.channels = nch;

    return 1;
}

static void convert_buffer(gpointer buffer, gint length)
{
    gint i;

    if (afmt == FMT_S8)
    {
        guint8 *ptr1 = buffer;
        gint8 *ptr2 = buffer;

        for (i = 0; i < length; i++)
            *(ptr1++) = *(ptr2++) ^ 128;
    }
    if (afmt == FMT_S16_BE)
    {
        gint16 *ptr = buffer;

        for (i = 0; i < length >> 1; i++, ptr++)
            *ptr = GUINT16_SWAP_LE_BE(*ptr);
    }
    if (afmt == FMT_S16_NE)
    {
        gint16 *ptr = buffer;

        for (i = 0; i < length >> 1; i++, ptr++)
            *ptr = GINT16_TO_LE(*ptr);
    }
    if (afmt == FMT_U16_BE)
    {
        gint16 *ptr1 = buffer;
        guint16 *ptr2 = buffer;

        for (i = 0; i < length >> 1; i++, ptr2++)
            *(ptr1++) = GINT16_TO_LE(GUINT16_FROM_BE(*ptr2) ^ 32768);
    }
    if (afmt == FMT_U16_LE)
    {
        gint16 *ptr1 = buffer;
        guint16 *ptr2 = buffer;

        for (i = 0; i < length >> 1; i++, ptr2++)
            *(ptr1++) = GINT16_TO_LE(GUINT16_FROM_LE(*ptr2) ^ 32768);
    }
    if (afmt == FMT_U16_NE)
    {
        gint16 *ptr1 = buffer;
        guint16 *ptr2 = buffer;

        for (i = 0; i < length >> 1; i++, ptr2++)
            *(ptr1++) = GINT16_TO_LE((*ptr2) ^ 32768);
    }
}

static void disk_write(void *ptr, gint length)
{
    AFormat new_format;
    int new_frequency, new_channels;
    EffectPlugin *ep;

    new_format = input.format;
    new_frequency = input.frequency;
    new_channels = input.channels;

    ep = get_current_effect_plugin();
    if ( effects_enabled() && ep && ep->query_format ) { 
        ep->query_format(&new_format,&new_frequency,&new_channels);
    }

    if ( effects_enabled() && ep && ep->mod_samples ) { 
        length = ep->mod_samples(&ptr,length,
                                 input.format,
                                 input.frequency,
                                 input.channels );
    }

    if (afmt == FMT_S8 || afmt == FMT_S16_BE ||
        afmt == FMT_U16_LE || afmt == FMT_U16_BE || afmt == FMT_U16_NE)
        convert_buffer(ptr, length);
#ifdef WORDS_BIGENDIAN
    if (afmt == FMT_S16_NE)
        convert_buffer(ptr, length);
#endif
    written += vfs_fwrite(ptr, 1, length, output_file);
}

static void disk_close(void)
{
    if (output_file)
    {
        header.length = GUINT32_TO_LE(written + sizeof (struct wavhead) - 8);

        header.data_length = GUINT32_TO_LE(written);
        vfs_fseek(output_file, 0, SEEK_SET);
        vfs_fwrite(&header, sizeof (struct wavhead), 1, output_file);

        vfs_fclose(output_file);
        written = 0;
    }
    output_file = NULL;
}

static void disk_flush(gint time)
{
    if (time == 0)
    {
        disk_close();
        disk_open(afmt, arate, ach);
    }
}

static void disk_pause(short p)
{
}

static gint disk_free(void)
{
    return 1000000;
}

static gint disk_playing(void)
{
    return 0;
}

static gint disk_get_written_time(void)
{
    if(header.byte_p_sec != 0)
        return (gint) ((written * 1000) / header.byte_p_sec);
    return 0;
}

static gint disk_get_output_time(void)
{
    return disk_get_written_time();
}

static void configure_ok_cb(gpointer data)
{
    ConfigDb *db;

    if (file_path)
        g_free(file_path);
    file_path = g_strdup(gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(path_dirbrowser)));

    use_suffix =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(use_suffix_toggle));

    db = bmp_cfg_db_open();
    bmp_cfg_db_set_string(db, "disk_writer", "file_path", file_path);
    bmp_cfg_db_set_bool(db, "disk_writer", "save_original", save_original);
    bmp_cfg_db_set_bool(db, "disk_writer", "use_suffix", use_suffix);
    bmp_cfg_db_close(db);

    gtk_widget_destroy(configure_win);
    if (path_dirbrowser)
        gtk_widget_destroy(path_dirbrowser);
}

static void saveplace_original_cb(GtkWidget *button, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(path_hbox, FALSE);
        save_original = TRUE;
    }
}

static void saveplace_custom_cb(GtkWidget *button, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(path_hbox, TRUE);
        save_original = FALSE;
    }
}

static void configure_destroy(void)
{
    if (path_dirbrowser)
        gtk_widget_destroy(path_dirbrowser);
}

static void disk_configure(void)
{
    GtkTooltips *use_suffix_tooltips;

    if (!configure_win)
    {
        configure_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

        gtk_signal_connect(GTK_OBJECT(configure_win), "destroy",
                           GTK_SIGNAL_FUNC(configure_destroy), NULL);
        gtk_signal_connect(GTK_OBJECT(configure_win), "destroy",
                           GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                           &configure_win);

        gtk_window_set_title(GTK_WINDOW(configure_win),
                             _("Disk Writer Configuration"));
        gtk_window_set_position(GTK_WINDOW(configure_win), GTK_WIN_POS_MOUSE);

        gtk_container_set_border_width(GTK_CONTAINER(configure_win), 10);

        configure_vbox = gtk_vbox_new(FALSE, 10);
        gtk_container_add(GTK_CONTAINER(configure_win), configure_vbox);

        saveplace_hbox = gtk_hbox_new(FALSE, 5);
        gtk_container_add(GTK_CONTAINER(configure_vbox), saveplace_hbox);

        saveplace = gtk_radio_button_new_with_label(NULL, _("Save into original directory"));
        g_signal_connect(G_OBJECT(saveplace), "toggled", G_CALLBACK(saveplace_original_cb), NULL);
        gtk_box_pack_start(GTK_BOX(saveplace_hbox), saveplace, FALSE, FALSE, 0);

        saveplace = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(saveplace),
                                                                _("Save into custom directory"));
        g_signal_connect(G_OBJECT(saveplace), "toggled", G_CALLBACK(saveplace_custom_cb), NULL);
        gtk_box_pack_start(GTK_BOX(saveplace_hbox), saveplace, FALSE, FALSE, 0);

        if (!save_original)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(saveplace), TRUE);



        path_hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), path_hbox, FALSE, FALSE, 0);

        path_label = gtk_label_new(_("Output file folder:"));
        gtk_box_pack_start(GTK_BOX(path_hbox), path_label, FALSE, FALSE, 0);

        path_dirbrowser =
            gtk_file_chooser_button_new ("Pick a folder",
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(path_dirbrowser),
                                            file_path);
        gtk_box_pack_start(GTK_BOX(path_hbox), path_dirbrowser, TRUE, TRUE, 0);

        if (save_original)
            gtk_widget_set_sensitive(path_hbox, FALSE);



        use_suffix_toggle = gtk_check_button_new_with_label(_("Don't strip file name extension"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_suffix_toggle), use_suffix);
        gtk_box_pack_start(GTK_BOX(configure_vbox), use_suffix_toggle, FALSE, FALSE, 0);
        use_suffix_tooltips = gtk_tooltips_new();
        gtk_tooltips_set_tip(use_suffix_tooltips, use_suffix_toggle, "If enabled, the extension from the original filename will not be stripped before adding the .wav extension to the end.", NULL);
        gtk_tooltips_enable(use_suffix_tooltips);

        configure_separator = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(configure_vbox), configure_separator,
                           FALSE, FALSE, 0);

        configure_bbox = gtk_hbutton_box_new();
        gtk_button_box_set_layout(GTK_BUTTON_BOX(configure_bbox),
                                  GTK_BUTTONBOX_END);
        gtk_button_box_set_spacing(GTK_BUTTON_BOX(configure_bbox), 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), configure_bbox,
                           FALSE, FALSE, 0);

        configure_ok = gtk_button_new_from_stock(GTK_STOCK_OK);
        gtk_signal_connect(GTK_OBJECT(configure_ok), "clicked",
                           GTK_SIGNAL_FUNC(configure_ok_cb), NULL);
        gtk_box_pack_start(GTK_BOX(configure_bbox), configure_ok,
                           TRUE, TRUE, 0);

        configure_cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
        gtk_signal_connect_object(GTK_OBJECT(configure_cancel), "clicked",
                                  GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                  GTK_OBJECT(configure_win));
        gtk_box_pack_start(GTK_BOX(configure_bbox), configure_cancel,
                           TRUE, TRUE, 0);

        gtk_widget_show_all(configure_win);
    }
}

static void disk_getvol(gint *l, gint *r)
{
    (*l) = lvol;
    (*r) = rvol;
}

static void disk_setvol(gint l, gint r)
{
    lvol = l;
    rvol = r;
}
