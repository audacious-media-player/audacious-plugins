/*
 *  Copyright 2006 Christian Birchinger <joker@netswarm.net>
 *
 *  Based on the XMMS plugin:
 *  Copyright 2000 Håvard Kvålen <havardk@sol.no>
 *
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
#include <glib.h>
#include <gtk/gtk.h>

#include <audacious/configdb.h>
#include <audacious/gtk-compat.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

static GTimer *timer;
static gulong offset_time, written;
static gint bps;
static gboolean real_time = TRUE;
static gboolean paused, started;
static GtkWidget *configurewin;
static struct {
    gint format;
    gint frequency;
    gint channels;
} input_format;

#define ELAPSED_TIME (offset_time + g_timer_elapsed(timer, NULL) * 1000)

static gboolean null_init (void)
{
    mcs_handle_t *db;
    db = aud_cfg_db_open();
    aud_cfg_db_get_bool(db, "null", "real_time", &real_time);
    aud_cfg_db_close(db);
    return TRUE;
}

static void null_about(void)
{
    static GtkWidget *about;
    gchar *about_text;

    if (about)
        return;

    about_text = g_strjoin("", _("Null output plugin "), VERSION,
    _(" by Christian Birchinger <joker@netswarm.net>\n"
      "based on the XMMS plugin by Håvard Kvål <havardk@xmms.org>"), NULL);

    audgui_simple_message (& about, GTK_MESSAGE_INFO, _("About Null Output"),
     about_text);

    g_free(about_text);
}

static void null_configure_ok_cb(GtkButton *w, gpointer data)
{
    mcs_handle_t *db;

    db = aud_cfg_db_open();
    real_time = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data));
    aud_cfg_db_set_bool(db, "null", "real_time", real_time);
    aud_cfg_db_close(db);
    gtk_widget_destroy(configurewin);
}


static void null_configure(void)
{
    GtkWidget *rt_btn, *ok_button, *cancel_button, *vbox, *bbox;

    if (configurewin)
        return;

    configurewin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(configurewin), _("Null output preferences"));
    gtk_window_set_type_hint(GTK_WINDOW(configurewin), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_resizable(GTK_WINDOW(configurewin), FALSE);
    gtk_window_set_position(GTK_WINDOW(configurewin), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(configurewin), 10);
    g_signal_connect(G_OBJECT(configurewin), "destroy",
            G_CALLBACK(gtk_widget_destroyed), &configurewin);

    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(configurewin), vbox);

    rt_btn = gtk_check_button_new_with_label(_("Run in real time"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rt_btn), real_time);
    gtk_box_pack_start(GTK_BOX(vbox), rt_btn, FALSE, FALSE, 0);
    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);
    ok_button = gtk_button_new_with_label(_("Ok"));
    cancel_button = gtk_button_new_with_label(_("Cancel"));
    gtk_widget_set_can_default(ok_button, TRUE);
    gtk_widget_set_can_default(cancel_button, TRUE);
    g_signal_connect_swapped(G_OBJECT(cancel_button), "clicked",
            G_CALLBACK(gtk_widget_destroy), configurewin);
    g_signal_connect(G_OBJECT(ok_button), "clicked",
            G_CALLBACK(null_configure_ok_cb), rt_btn);
    gtk_box_pack_start(GTK_BOX(bbox), ok_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bbox), cancel_button, FALSE, FALSE, 0);
    gtk_widget_grab_default(ok_button);

    gtk_widget_show_all(configurewin);
}

static int null_open(gint fmt, int rate, int nch)
{
    offset_time = 0;
    written = 0;
    started = FALSE;
    paused = FALSE;
    input_format.format = fmt;
    input_format.frequency = rate;
    input_format.channels = nch;
    bps = rate * nch;
    switch (fmt)
    {
        case FMT_U8:
        case FMT_S8:
            break;
        default:
            bps <<= 1;
            break;
    }
    if (real_time)
        timer = g_timer_new();
    return 1;
}

static void null_write(void *ptr, int length)
{
    if (timer && !started)
    {
        g_timer_start(timer);
        started = TRUE;
    }

    written += length;
}

static void null_close(void)
{
    if (timer)
        g_timer_destroy(timer);
    timer = NULL;
}

static void null_flush(int time)
{
    offset_time = time;
    written = ((double)time * bps) / 1000;
    if (timer)
        g_timer_reset(timer);
}

static void null_pause (gboolean p)
{
    paused = p;
    if (!timer)
        return;

    if (paused)
        g_timer_stop(timer);
    else
    {
        offset_time += g_timer_elapsed(timer, NULL) * 1000;
        g_timer_start(timer);
    }
}

static int null_buffer_free(void)
{
    if (timer)
    {
        return 10240 - (written - (ELAPSED_TIME * bps) / 1000);
    }
    else
    {
        if (!paused)
            return 10000;
        else
            return 0;
    }
}

#if 0
static int null_playing(void)
{
    if (!timer)
        return FALSE;

    if ((gdouble)(written * 1000) / bps > ELAPSED_TIME)
        return TRUE;
    return FALSE;
}
#endif

static int null_get_written_time(void)
{
    if (!bps)
        return 0;
    return ((gint64)written * 1000) / bps;
}

static int null_get_output_time(void)
{
    if (!timer)
        return null_get_written_time();

    return ELAPSED_TIME;
}

AUD_OUTPUT_PLUGIN
(
 .name = "No Output",
 .probe_priority = 0,
 .init = null_init,
 .about = null_about,
 .configure = null_configure,
 .open_audio = null_open,
 .write_audio = null_write,
 .close_audio = null_close,
 .flush = null_flush,
 .pause = null_pause,
 .buffer_free = null_buffer_free,
 .output_time = null_get_output_time,
 .written_time = null_get_written_time,
 )
