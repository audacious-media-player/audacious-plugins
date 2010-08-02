/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2010 Audacious development team
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>

#include <audacious/drct.h>
#include <audacious/playlist.h>
#include <libaudcore/hook.h>

#include "ui_statusbar.h"

static void
ui_statusbar_update_playlist_length(gpointer unused, GtkWidget *label)
{
    gint playlist = aud_playlist_get_active();
    gint64 selection, total;
    gchar *sel_text, *tot_text, *text;

    total = aud_playlist_get_total_length (playlist, TRUE) / 1000;
    selection = aud_playlist_get_selected_length (playlist, TRUE) / 1000;

    if (selection >= 3600)
        sel_text = g_strdup_printf ("%" PRId64 ":%02" PRId64 ":%02" PRId64,
        selection / 3600, selection / 60 % 60, selection % 60);
    else
        sel_text = g_strdup_printf ("%" PRId64 ":%02" PRId64,
        selection / 60, selection % 60);

    if (total >= 3600)
        tot_text = g_strdup_printf ("%" PRId64 ":%02" PRId64 ":%02" PRId64,
        total / 3600, total / 60 % 60, total % 60);
    else
        tot_text = g_strdup_printf ("%" PRId64 ":%02" PRId64,
        total / 60, total % 60);

    text = g_strconcat(sel_text, "/", tot_text, NULL);
    gtk_label_set_text(GTK_LABEL(label), text);

    g_free(text);
    g_free(tot_text);
    g_free(sel_text);
}

static void
ui_statusbar_info_change(gpointer unused, GtkWidget *label)
{
    gint bitrate, samplerate, channels;
    gchar *text, *ch_text;
    const Tuple *tuple;
    gint playlist, entry;
    const gchar *codec;

    /* may be called asynchronously */
    if (!aud_drct_get_playing())
        return;

    playlist = aud_playlist_get_active();
    entry = aud_playlist_get_position(playlist);
    tuple = aud_playlist_entry_get_tuple (playlist, entry, FALSE);
    codec = tuple != NULL ? tuple_get_string(tuple, FIELD_CODEC, NULL) : "???";

    aud_drct_get_info(&bitrate, &samplerate, &channels);

    switch (channels)
    {
    case 1:
        ch_text = g_strdup(_("mono"));
        break;
    case 2:
        ch_text = g_strdup(_("stereo"));
        break;
    default:
        ch_text = g_strdup_printf(_("%d channels"), channels);
        break;
    }

    text = g_strdup_printf(_("%s: %d kbps, %d Hz, %s"), codec, bitrate / 1000, samplerate, ch_text);
    gtk_label_set_text(GTK_LABEL(label), text);

    g_free(text);
    g_free(ch_text);
}

static void
ui_statusbar_playback_stop(gpointer unused, GtkWidget *label)
{
    gtk_label_set_text(GTK_LABEL(label), "");
}

static void ui_statusbar_destroy_cb(GtkWidget *widget, gpointer user_data)
{
    hook_dissociate("info change", (HookFunction) ui_statusbar_info_change);
    hook_dissociate("playback stop", (HookFunction) ui_statusbar_playback_stop);
    hook_dissociate("playlist update", (HookFunction) ui_statusbar_update_playlist_length);
    hook_dissociate("info change", (HookFunction) ui_statusbar_update_playlist_length);
}

GtkWidget *
ui_statusbar_new(void)
{
    GtkWidget *hbox;
    GtkWidget *status, *length;

    hbox = gtk_hbox_new(FALSE, 3);

    status = gtk_widget_new(GTK_TYPE_LABEL, "xalign", 0.0, NULL);
    gtk_label_set_ellipsize ((GtkLabel *) status, PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(hbox), status, TRUE, TRUE, 5);

    hook_associate("info change", (HookFunction) ui_statusbar_info_change, status);
    hook_associate("playback stop", (HookFunction) ui_statusbar_playback_stop, status);

    length = gtk_widget_new(GTK_TYPE_LABEL, "xalign", 1.0, NULL);
    gtk_box_pack_start(GTK_BOX(hbox), length, FALSE, FALSE, 5);
    ui_statusbar_update_playlist_length(NULL, length);

    hook_associate("playlist update", (HookFunction) ui_statusbar_update_playlist_length, length);
    hook_associate("info change", (HookFunction) ui_statusbar_update_playlist_length, length);

    g_signal_connect(G_OBJECT(hbox), "destroy", G_CALLBACK(ui_statusbar_destroy_cb), NULL);

    if (aud_drct_get_playing())
    {
        ui_statusbar_info_change(NULL, status);
    }

    return hbox;
}
