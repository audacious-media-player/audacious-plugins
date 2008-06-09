/*  Icecast-Plugin
 *  (C) copyright 2008 based of FileWriter-plugin
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

#include "../filewriter/filewriter.h"
#include "../filewriter/plugins.h"

struct format_info input;

static GtkWidget *configure_win = NULL, *configure_vbox;
static GtkWidget *addr_hbox, *addr_label, *addr_entry;
static GtkWidget *configure_bbox, *configure_ok, *configure_cancel;

static GtkWidget *streamformat_hbox, *streamformat_label, *streamformat_combo, *plugin_button;

enum streamformat_t
{
#ifdef FILEWRITER_MP3
    MP3,
#endif
#ifdef FILEWRITER_VORBIS
    VORBIS,
#endif
    streamformat_MAX
};

static gint streamformat = VORBIS;

static FileWriter plugin;

static gchar *server_address = NULL;

VFSFile *output_file = NULL;
guint64 written = 0;
guint64 offset = 0;
Tuple *tuple = NULL;

static void ice_init(void);
static void ice_about(void);
static gint ice_open(AFormat fmt, gint rate, gint nch);
static void ice_write(void *ptr, gint length);
static gint ice_write_output(void *ptr, gint length);
static void ice_close(void);
static void ice_flush(gint time);
static void ice_pause(short p);
static gint ice_free(void);
static gint ice_playing(void);
static gint ice_get_written_time(void);
static gint ice_get_output_time(void);
static void ice_configure(void);
/*static int ice_mod_samples(gpointer * d, gint length, AFormat afmt, gint srate, gint nch);*/

OutputPlugin ice_op =
{
    .description = "Icecast Plugin",
    .init = ice_init,
    .about = ice_about,
    .configure = ice_configure,
    .open_audio = ice_open,
    .write_audio = ice_write,
    .close_audio = ice_close,
    .flush = ice_flush,
    .pause = ice_pause,
    .buffer_free = ice_free,
    .buffer_playing = ice_playing,
    .output_time = ice_get_output_time,
    .written_time = ice_get_written_time
};
/*
EffectPlugin ice_ep =
{
        .description = "Icecast Plugin",
        .init = ice_init,
        .cleanup = ice_cleanup,
        .about = ice_about,
        .configure = ice_configure,
        .mod_samples = ice_mod_samples,
};
*/
OutputPlugin *ice_oplist[] = { &ice_op, NULL };

SIMPLE_OUTPUT_PLUGIN(icecast, ice_oplist);

static void set_plugin(void)
{
    if (streamformat < 0 || streamformat >= streamformat_MAX)
        streamformat = 0;

#ifdef FILEWRITER_MP3
    if (streamformat == MP3)
        plugin = mp3_plugin;
#endif
#ifdef FILEWRITER_VORBIS
    if (streamformat == VORBIS)
        plugin = vorbis_plugin;
#endif
}

static void ice_init(void)
{
    ConfigDb *db;
    puts("ICE_INIT");

    db = aud_cfg_db_open();
    aud_cfg_db_get_int(db, "icecast", "streamformat", &streamformat);
    aud_cfg_db_get_string(db, "icecast", "server_address", &server_address);
    aud_cfg_db_close(db);

    set_plugin();
    if (plugin.init)
        plugin.init(&ice_write_output);
}

void ice_about(void)
{
    static GtkWidget *dialog;

    if (dialog != NULL)
        return;

    dialog = audacious_info_dialog(_("About Icecast-Plugin"),
                               _("Icecast-Plugin\n\n"
                               "This program is free software; you can redistribute it and/or modify\n"
                               "it under the terms of the GNU General Public License as published by\n"
                               "the Free Software Foundation; either version 2 of the License, or\n"
                               "(at your option) any later version.\n"
                               "\n"
                               "This program is distributed in the hope that it will be useful,\n"
                               "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                               "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                               "GNU General Public License for more details.\n"
                               "\n"
                               "You should have received a copy of the GNU General Public License\n"
                               "along with this program; if not, write to the Free Software\n"
                               "Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,\n"
                               "USA."), _("Ok"), FALSE, NULL, NULL);
    gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
                       GTK_SIGNAL_FUNC(gtk_widget_destroyed), &dialog);
}

static gint ice_open(AFormat fmt, gint rate, gint nch)
{
    gint rv;

    input.format = fmt;
    input.frequency = rate;
    input.channels = nch;

    rv = (plugin.open)();

    puts("ICE_OPEN");
    return rv;
}

static void convert_buffer(gpointer buffer, gint length)
{
    gint i;

    if (input.format == FMT_S8)
    {
        guint8 *ptr1 = buffer;
        gint8 *ptr2 = buffer;

        for (i = 0; i < length; i++)
            *(ptr1++) = *(ptr2++) ^ 128;
    }
    if (input.format == FMT_S16_BE)
    {
        gint16 *ptr = buffer;

        for (i = 0; i < length >> 1; i++, ptr++)
            *ptr = GUINT16_SWAP_LE_BE(*ptr);
    }
    if (input.format == FMT_S16_NE)
    {
        gint16 *ptr = buffer;

        for (i = 0; i < length >> 1; i++, ptr++)
            *ptr = GINT16_TO_LE(*ptr);
    }
    if (input.format == FMT_U16_BE)
    {
        gint16 *ptr1 = buffer;
        guint16 *ptr2 = buffer;

        for (i = 0; i < length >> 1; i++, ptr2++)
            *(ptr1++) = GINT16_TO_LE(GUINT16_FROM_BE(*ptr2) ^ 32768);
    }
    if (input.format == FMT_U16_LE)
    {
        gint16 *ptr1 = buffer;
        guint16 *ptr2 = buffer;

        for (i = 0; i < length >> 1; i++, ptr2++)
            *(ptr1++) = GINT16_TO_LE(GUINT16_FROM_LE(*ptr2) ^ 32768);
    }
    if (input.format == FMT_U16_NE)
    {
        gint16 *ptr1 = buffer;
        guint16 *ptr2 = buffer;

        for (i = 0; i < length >> 1; i++, ptr2++)
            *(ptr1++) = GINT16_TO_LE((*ptr2) ^ 32768);
    }
}

static void ice_write(void *ptr, gint length)
{
    if (input.format == FMT_S8 || input.format == FMT_S16_BE ||
        input.format == FMT_U16_LE || input.format == FMT_U16_BE ||
        input.format == FMT_U16_NE)
        convert_buffer(ptr, length);
#ifdef WORDS_BIGENDIAN
    if (input.format == FMT_S16_NE)
        convert_buffer(ptr, length);
#endif

    plugin.write(ptr, length);
}

static gint ice_write_output(void *ptr, gint length)
{
    int i;
    printf("ice_write(");
    for (i=0;(i<length)&&(i<16);i++) printf("%.2x",((char*)ptr)[i]);
    printf(")\n");
    return length;
}

static void ice_close(void)
{
    plugin.close();

    if (output_file)
    {
        written = 0;
        aud_vfs_fclose(output_file);
    }
    output_file = NULL;
    puts("ICE_CLOSE");
}

static void ice_flush(gint time)
{
    if (time < 0)
        return;

    ice_close();
    ice_open(input.format, input.frequency, input.channels);

    offset = time;
}

static void ice_pause(short p)
{
}

static gint ice_free(void)
{
    return plugin.free();
}

static gint ice_playing(void)
{
    return plugin.playing();
}

static gint ice_get_written_time(void)
{
    return plugin.get_written_time();
}

static gint ice_get_output_time(void)
{
    return ice_get_written_time();
}

static void configure_ok_cb(gpointer data)
{
    ConfigDb *db;

    streamformat = gtk_combo_box_get_active(GTK_COMBO_BOX(streamformat_combo));

    g_free(server_address);
    server_address = g_strdup(gtk_entry_get_text(GTK_ENTRY(addr_entry)));

    db = aud_cfg_db_open();
    aud_cfg_db_set_int(db, "icecast", "streamformat", streamformat);
    aud_cfg_db_set_string(db, "icecast", "server_address", server_address);

    aud_cfg_db_close(db);

    gtk_widget_destroy(configure_win);
}

static void streamformat_cb(GtkWidget *combo, gpointer data)
{
    streamformat = gtk_combo_box_get_active(GTK_COMBO_BOX(streamformat_combo));
    set_plugin();
    if (plugin.init)
        plugin.init(&ice_write_output);

    gtk_widget_set_sensitive(plugin_button, plugin.configure != NULL);
}

static void plugin_configure_cb(GtkWidget *button, gpointer data)
{
    if (plugin.configure)
        plugin.configure();
}

static void configure_destroy(void)
{
}

static void ice_configure(void)
{
    if (!configure_win)
    {
        configure_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

        gtk_signal_connect(GTK_OBJECT(configure_win), "destroy",
                           GTK_SIGNAL_FUNC(configure_destroy), NULL);
        gtk_signal_connect(GTK_OBJECT(configure_win), "destroy",
                           GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                           &configure_win);

        gtk_window_set_title(GTK_WINDOW(configure_win),
                             _("Icecast Configuration"));
        gtk_window_set_position(GTK_WINDOW(configure_win), GTK_WIN_POS_MOUSE);

        gtk_container_set_border_width(GTK_CONTAINER(configure_win), 10);

        configure_vbox = gtk_vbox_new(FALSE, 10);
        gtk_container_add(GTK_CONTAINER(configure_win), configure_vbox);


        streamformat_hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), streamformat_hbox, FALSE, FALSE, 0);

        streamformat_label = gtk_label_new(_("Output stream format:"));
        gtk_box_pack_start(GTK_BOX(streamformat_hbox), streamformat_label, FALSE, FALSE, 0);

        streamformat_combo = gtk_combo_box_new_text();
#ifdef FILEWRITER_MP3
        gtk_combo_box_append_text(GTK_COMBO_BOX(streamformat_combo), "MP3");
#endif
#ifdef FILEWRITER_VORBIS
        gtk_combo_box_append_text(GTK_COMBO_BOX(streamformat_combo), "Vorbis");
#endif
        gtk_box_pack_start(GTK_BOX(streamformat_hbox), streamformat_combo, FALSE, FALSE, 0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(streamformat_combo), streamformat);
        g_signal_connect(G_OBJECT(streamformat_combo), "changed", G_CALLBACK(streamformat_cb), NULL);

        plugin_button = gtk_button_new_with_label(_("Configure"));
        gtk_widget_set_sensitive(plugin_button, plugin.configure != NULL);
        g_signal_connect(G_OBJECT(plugin_button), "clicked", G_CALLBACK(plugin_configure_cb), NULL);
        gtk_box_pack_end(GTK_BOX(streamformat_hbox), plugin_button, FALSE, FALSE, 0);




        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

        addr_hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), addr_hbox, FALSE, FALSE, 0);

        addr_label = gtk_label_new(_("Server address:"));
        gtk_box_pack_start(GTK_BOX(addr_hbox), addr_label, FALSE, FALSE, 0);

        addr_entry = gtk_entry_new();

	gtk_entry_set_text(GTK_ENTRY(addr_entry), server_address);

        gtk_box_pack_start(GTK_BOX(addr_hbox), addr_entry, TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

        configure_bbox = gtk_hbutton_box_new();
        gtk_button_box_set_layout(GTK_BUTTON_BOX(configure_bbox),
                                  GTK_BUTTONBOX_END);
        gtk_button_box_set_spacing(GTK_BUTTON_BOX(configure_bbox), 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), configure_bbox,
                           FALSE, FALSE, 0);

        configure_cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
        gtk_signal_connect_object(GTK_OBJECT(configure_cancel), "clicked",
                                  GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                  GTK_OBJECT(configure_win));
        gtk_box_pack_start(GTK_BOX(configure_bbox), configure_cancel,
                           TRUE, TRUE, 0);

        configure_ok = gtk_button_new_from_stock(GTK_STOCK_OK);
        gtk_signal_connect(GTK_OBJECT(configure_ok), "clicked",
                           GTK_SIGNAL_FUNC(configure_ok_cb), NULL);
        gtk_box_pack_start(GTK_BOX(configure_bbox), configure_ok,
                           TRUE, TRUE, 0);

        gtk_widget_show_all(configure_win);
    }
}
