/*  Icecast-Plugin
 *  (C) copyright 2008 Andrew O. Shadoura
 *  Based on FileWriter-plugin
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

#include "../filewriter/filewriter.h"
#include "../filewriter/plugins.h"
#include <shout/shout.h>

struct format_info input;

static GtkWidget *configure_win = NULL, *configure_vbox;
static GtkWidget *addr_entry, *port_spin, *timeout_spin, *buffersize_spin, *bufferflush_spin;
static GtkWidget *user_entry, *password_entry, *mount_entry;
static GtkWidget *configure_bbox, *configure_ok, *configure_cancel;
static guint ice_tid=0;

static gint ice_close_timeout;

static GtkWidget *streamformat_combo, *plugin_button;

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

static unsigned int streamformat_shout[] =
{
#ifdef FILEWRITER_MP3
    SHOUT_FORMAT_MP3,
#endif
#ifdef FILEWRITER_VORBIS
    SHOUT_FORMAT_OGG
#endif
};

static FileWriter plugin;
static FileWriter plugin_new;
static uint8_t *outputbuffer = NULL;
static guint outputlength=0;
static gint buffersize;
static gint bufferflush;
static gint buffersize_new;
static gint bufferflush_new;
static gdouble bufferflushperc;
static gchar *server_address = NULL;
static gint server_port=8000;

static gchar *server_user = NULL;
static gchar *server_password = NULL;
static gchar *mountpoint = NULL;

VFSFile *output_file = NULL;
guint64 written = 0;
guint64 offset = 0;
Tuple *tuple = NULL;
static shout_t *shout = NULL;
static gboolean paused = FALSE;

static void ice_init(void);
static void ice_cleanup(void);
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
    .cleanup = ice_cleanup,
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
        plugin_new = mp3_plugin;
#endif
#ifdef FILEWRITER_VORBIS
    if (streamformat == VORBIS)
        plugin_new = vorbis_plugin;
#endif
}

static void ice_init(void)
{
    ConfigDb *db;
    g_debug("ICE_INIT");
    shout_init();
    g_message("Using libshout %s", shout_version(NULL, NULL, NULL));

    db = aud_cfg_db_open();
    aud_cfg_db_get_int(db, ICECAST_CFGID, "streamformat", &streamformat);
    aud_cfg_db_get_string(db, ICECAST_CFGID, "server_address", &server_address);
    aud_cfg_db_get_int(db, ICECAST_CFGID, "server_port", &server_port);
    if (!server_port) server_port=8000;
    aud_cfg_db_get_int(db, ICECAST_CFGID, "timeout", &ice_close_timeout);
    if (!ice_close_timeout) ice_close_timeout=5;
    aud_cfg_db_get_int(db, ICECAST_CFGID, "buffersize", &buffersize);
    if (!buffersize) buffersize=8192;
    buffersize_new=buffersize;
    aud_cfg_db_get_double(db, ICECAST_CFGID, "bufferflush", &bufferflushperc);
    if (!bufferflushperc) bufferflushperc=80.0;
    bufferflush=(gint)(buffersize*bufferflushperc);
    bufferflush_new=bufferflush;
    aud_cfg_db_get_string(db, ICECAST_CFGID, "server_user", &server_user);
    aud_cfg_db_get_string(db, ICECAST_CFGID, "server_password", &server_password);
    aud_cfg_db_get_string(db, ICECAST_CFGID, "mountpoint", &mountpoint);
    aud_cfg_db_close(db);

    outputbuffer=g_try_malloc(buffersize);

    set_plugin();
    plugin=plugin_new;
    if (plugin.init)
        plugin.init(&ice_write_output);
}

static void ice_cleanup(void)
{
    if (shout)
    {
        shout_close(shout);
    }
    if (outputbuffer)
        g_free(outputbuffer);
    shout_shutdown();
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
        "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,\n"
        "USA."), _("Ok"), FALSE, NULL, NULL);
    gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
        GTK_SIGNAL_FUNC(gtk_widget_destroyed), &dialog);
}

static gint ice_open(AFormat fmt, gint rate, gint nch)
{
    gint rv;
    gint pos;
    Playlist *playlist;

    if (buffersize!=buffersize_new)
    {
        buffersize=buffersize_new;
        outputbuffer=g_try_realloc(outputbuffer, buffersize);
    }

    if (bufferflush!=bufferflush_new)
        bufferflush=bufferflush_new;

    if (!outputbuffer)
        return 0;

    if (ice_tid)
    {
	g_source_remove(ice_tid);
	ice_tid = 0;
    }

    input.format = fmt;
    input.frequency = rate;
    input.channels = nch;

    playlist = aud_playlist_get_active();
    if (!playlist)
        return 0;

    pos = aud_playlist_get_position(playlist);
    tuple = aud_playlist_get_tuple(playlist, pos);

    plugin = plugin_new;

    if (!shout)
    {
        rv = (plugin.open)();

        if (!(shout = shout_new()))
            return 0;

        if (shout_set_host(shout, server_address) != SHOUTERR_SUCCESS)
        {
            g_warning("Error setting hostname: %s\n", shout_get_error(shout));
            return 0;
        }

        if (shout_set_protocol(shout, SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS)
        {
            g_warning("Error setting protocol: %s\n", shout_get_error(shout));
            return 0;
        }

        if (shout_set_port(shout, server_port) != SHOUTERR_SUCCESS)
        {
            g_warning("Error setting port: %s\n", shout_get_error(shout));
            return 0;
        }

        if (shout_set_password(shout, server_password) != SHOUTERR_SUCCESS)
        {
            g_warning("Error setting password: %s\n", shout_get_error(shout));
            return 0;
        }

        if (shout_set_mount(shout, mountpoint) != SHOUTERR_SUCCESS)
        {
            g_warning("Error setting mount: %s\n", shout_get_error(shout));
            return 0;
        }

        if (shout_set_user(shout, server_user) != SHOUTERR_SUCCESS)
        {
            g_warning("Error setting user: %s\n", shout_get_error(shout));
            return 0;
        }

        if (shout_set_format(shout, streamformat_shout[streamformat]) != SHOUTERR_SUCCESS)
        {
            g_warning("Error setting user: %s\n", shout_get_error(shout));
            return 0;
        }

        if (shout_open(shout) != SHOUTERR_SUCCESS)
        {
            g_warning("Error connecting to server: %s\n", shout_get_error(shout));
            return 0;
        }
    }
    else
        rv = 1;

    {
        shout_metadata_t *sm = NULL;
        sm = shout_metadata_new();
        if (sm)
        {
            shout_metadata_add(sm, "charset", "UTF-8");
            shout_metadata_add(sm, "title", aud_tuple_get_string(tuple, FIELD_TITLE, NULL));
            shout_metadata_add(sm, "artist", aud_tuple_get_string(tuple, FIELD_ARTIST, NULL));
            shout_set_metadata(shout, sm);
            shout_metadata_free(sm);
        }
    }

    g_debug("ICE_OPEN");
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

static gint ice_real_write(void* ptr, gint length)
{
    gint ret;
    if (!length) return length;
    ret = shout_send(shout, ptr, length);
    shout_sync(shout);
    g_debug("ice_write[%d:%d](", ret, length);
    {
        gint i;
        for (i=0;(i<length)&&(i<16);i++) g_debug("%c",g_ascii_isprint(((char*)ptr)[i])?(((char*)ptr)[i]):'.');
    }
    g_debug(")");
    return 0;
}

static gint ice_write_output(void *ptr, gint length)
{
    if ((!shout) || (!length)) return 0;
    g_debug("outputlength=%d, length=%d...",outputlength, length);
    if ((outputlength>bufferflush)||((outputlength+length)>buffersize))
    {
        g_debug("flushing");
        outputlength=ice_real_write(outputbuffer, outputlength);
    }
    {
        if (length>buffersize)
        {
            g_debug("data too long, flushing");
            ice_real_write(ptr, length);
        }
        else
        {
            g_debug("adding");
            memcpy(&(outputbuffer[outputlength]), ptr, length);
            outputlength+=length;
        }
    }
    return length;
}

static gboolean ice_real_close(gpointer data)
{
    plugin.close();

    if (shout)
    {
        written = 0;
        shout_close(shout);
    }
    shout = NULL;
    ice_tid=0;
    g_debug("ICE_REAL_CLOSE");
    return FALSE;
}


static void ice_close(void)
{
    if (ice_tid) g_source_remove(ice_tid);
    ice_tid=g_timeout_add_seconds(3, ice_real_close, NULL);
    g_debug("ICE_CLOSE: starting timer");
}

static void ice_flush(gint time)
{
    if (time < 0)
        return;

    plugin.flush();
    ice_open(input.format, input.frequency, input.channels);

    offset = time;
}

static void ice_pause(short p)
{
    paused = p;
}

static gint ice_free(void)
{
    return paused?0:plugin.free();
}

static gint ice_playing(void)
{
    return !paused;
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

    g_free(server_user);
    server_user = g_strdup(gtk_entry_get_text(GTK_ENTRY(user_entry)));

    g_free(server_password);
    server_password = g_strdup(gtk_entry_get_text(GTK_ENTRY(password_entry)));

    server_port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(port_spin));

    ice_close_timeout = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(timeout_spin));

    buffersize_new = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(buffersize_spin));

    bufferflushperc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(bufferflush_spin));
    bufferflush_new=(gint)(buffersize*bufferflushperc);

    g_free(mountpoint);
    mountpoint = g_strdup(gtk_entry_get_text(GTK_ENTRY(mount_entry)));

    db = aud_cfg_db_open();
    aud_cfg_db_set_int(db, ICECAST_CFGID, "streamformat", streamformat);
    aud_cfg_db_set_string(db, ICECAST_CFGID, "server_address", server_address);
    aud_cfg_db_set_string(db, ICECAST_CFGID, "server_user", server_user);
    aud_cfg_db_set_string(db, ICECAST_CFGID, "server_password", server_password);
    aud_cfg_db_set_int(db, ICECAST_CFGID, "server_port", server_port);
    aud_cfg_db_set_int(db, ICECAST_CFGID, "timeout", ice_close_timeout);
    aud_cfg_db_set_int(db, ICECAST_CFGID, "buffersize", buffersize_new);
    aud_cfg_db_set_double(db, ICECAST_CFGID, "bufferflush", bufferflushperc);
    aud_cfg_db_set_string(db, ICECAST_CFGID, "mountpoint", mountpoint);

    aud_cfg_db_close(db);

    gtk_widget_destroy(configure_win);
}

static void streamformat_cb(GtkWidget *combo, gpointer data)
{
    streamformat = gtk_combo_box_get_active(GTK_COMBO_BOX(streamformat_combo));
    set_plugin();
    if (plugin_new.init)
        plugin_new.init(&ice_write_output);

    gtk_widget_set_sensitive(plugin_button, plugin_new.configure != NULL);
}

static void plugin_configure_cb(GtkWidget *button, gpointer data)
{
    if (plugin_new.configure)
        plugin_new.configure();
}

static void configure_destroy(void)
{
}

static void ice_configure(void)
{
    if (!configure_win)
    {
        GtkWidget * hbox;
        GtkWidget * label;

        plugin_new = plugin;

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

        hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), hbox, FALSE, FALSE, 0);

        label = gtk_label_new(_("Output stream format:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        streamformat_combo = gtk_combo_box_new_text();
#ifdef FILEWRITER_MP3
        gtk_combo_box_append_text(GTK_COMBO_BOX(streamformat_combo), "MP3");
#endif
#ifdef FILEWRITER_VORBIS
        gtk_combo_box_append_text(GTK_COMBO_BOX(streamformat_combo), "Vorbis");
#endif
        gtk_box_pack_start(GTK_BOX(hbox), streamformat_combo, FALSE, FALSE, 0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(streamformat_combo), streamformat);
        g_signal_connect(G_OBJECT(streamformat_combo), "changed", G_CALLBACK(streamformat_cb), NULL);

        plugin_button = gtk_button_new_with_label(_("Configure"));
        gtk_widget_set_sensitive(plugin_button, plugin_new.configure != NULL);
        g_signal_connect(G_OBJECT(plugin_button), "clicked", G_CALLBACK(plugin_configure_cb), NULL);
        gtk_box_pack_end(GTK_BOX(hbox), plugin_button, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

        hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), hbox, FALSE, FALSE, 0);

        label = gtk_label_new(_("Server address:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        addr_entry = gtk_entry_new();

	gtk_entry_set_text(GTK_ENTRY(addr_entry), server_address);

        gtk_box_pack_start(GTK_BOX(hbox), addr_entry, TRUE, TRUE, 0);

        port_spin = gtk_spin_button_new_with_range(0.0, 65535.0, 1.0);

        gtk_spin_button_set_digits(GTK_SPIN_BUTTON(port_spin), 0);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(port_spin), (gdouble)server_port);

        gtk_box_pack_start(GTK_BOX(hbox), port_spin, TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

        hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), hbox, FALSE, FALSE, 0);

        label = gtk_label_new(_("Mount point:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        mount_entry = gtk_entry_new();

	gtk_entry_set_text(GTK_ENTRY(mount_entry), mountpoint);

        gtk_box_pack_start(GTK_BOX(hbox), mount_entry, TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

        hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), hbox, FALSE, FALSE, 0);

        label = gtk_label_new(_("User name:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        user_entry = gtk_entry_new();

	gtk_entry_set_text(GTK_ENTRY(user_entry), server_user);

        gtk_box_pack_start(GTK_BOX(hbox), user_entry, TRUE, TRUE, 0);

        label = gtk_label_new(_("Password:"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        password_entry = gtk_entry_new();

	gtk_entry_set_text(GTK_ENTRY(password_entry), server_password);

        gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);

        gtk_box_pack_start(GTK_BOX(hbox), password_entry, TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

        hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), hbox, FALSE, FALSE, 0);

        label = gtk_label_new(_("Connection timeout (seconds):"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        timeout_spin = gtk_spin_button_new_with_range(1.0, 65535.0, 1.0);

        gtk_spin_button_set_digits(GTK_SPIN_BUTTON(timeout_spin), 0);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(timeout_spin), (gdouble)ice_close_timeout);

        gtk_box_pack_start(GTK_BOX(hbox), timeout_spin, TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

        hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), hbox, FALSE, FALSE, 0);

        label = gtk_label_new(_("Buffer size (bytes):"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        buffersize_spin = gtk_spin_button_new_with_range(1.0, 65535.0, 1.0);

        gtk_spin_button_set_digits(GTK_SPIN_BUTTON(buffersize_spin), 0);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(buffersize_spin), (gdouble)buffersize);

        gtk_box_pack_start(GTK_BOX(hbox), buffersize_spin, TRUE, TRUE, 0);

        hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), hbox, FALSE, FALSE, 0);

        label = gtk_label_new(_("Flush buffer if "));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        bufferflush_spin = gtk_spin_button_new_with_range(1.0, 65535.0, 1.0);

        gtk_spin_button_set_digits(GTK_SPIN_BUTTON(bufferflush_spin), 0);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(bufferflush_spin), bufferflushperc);

        gtk_box_pack_start(GTK_BOX(hbox), bufferflush_spin, TRUE, TRUE, 0);

        label = gtk_label_new(_("percents are filled"));
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

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
