/*
 * ui_statusbar.c
 * Copyright 2010-2011 William Pitcock, Michał Lipski, and John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <glib.h>
#include <audacious/i18n.h>
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

#define APPEND(b, ...) snprintf (b + strlen (b), sizeof b - strlen (b), __VA_ARGS__)

static void ui_statusbar_update_playlist_length (void * unused, GtkWidget * label)
{
    int playlist = aud_playlist_get_active ();
    int64_t selection = aud_playlist_get_selected_length (playlist) / 1000;
    int64_t total = aud_playlist_get_total_length (playlist) / 1000;

    char buf[64];
    buf[0] = 0;

    if (selection >= 3600)
        APPEND (buf, "%" PRId64 ":%02" PRId64 ":%02" PRId64,
         selection / 3600, selection / 60 % 60, selection % 60);
    else
        APPEND (buf, "%" PRId64 ":%02" PRId64,
         selection / 60, selection % 60);

    if (total >= 3600)
        APPEND (buf, "/%" PRId64 ":%02" PRId64 ":%02" PRId64,
         total / 3600, total / 60 % 60, total % 60);
    else
        APPEND (buf, "/%" PRId64 ":%02" PRId64,
         total / 60, total % 60);

    gtk_label_set_text ((GtkLabel *) label, buf);
}

static void ui_statusbar_info_change (void * unused, GtkWidget * label)
{
    /* may be called asynchronously */
    if (! aud_drct_get_playing ())
        return;

    int playlist = aud_playlist_get_playing ();
    Tuple * tuple = aud_playlist_entry_get_tuple (playlist,
     aud_playlist_get_position (playlist), FALSE);
    char * codec = tuple ? tuple_get_str (tuple, FIELD_CODEC) : NULL;
    if (tuple)
        tuple_unref (tuple);

    int bitrate, samplerate, channels;
    aud_drct_get_info (& bitrate, & samplerate, & channels);

    char buf[256];
    buf[0] = 0;

    if (codec)
    {
        APPEND (buf, "%s", codec);
        if (channels > 0 || samplerate > 0 || bitrate > 0)
            APPEND (buf, ", ");
    }

    str_unref (codec);

    if (channels > 0)
    {
        if (channels == 1)
            APPEND (buf, _("mono"));
        else if (channels == 2)
            APPEND (buf, _("stereo"));
        else
            APPEND (buf, ngettext ("%d channel", "%d channels", channels),
             channels);

        if (samplerate > 0 || bitrate > 0)
            APPEND (buf, ", ");
    }

    if (samplerate > 0)
    {
        APPEND (buf, "%d kHz", samplerate / 1000);
        if (bitrate > 0)
            APPEND (buf, ", ");
    }

    if (bitrate > 0)
        APPEND (buf, _("%d kbps"), bitrate / 1000);

    gtk_label_set_text ((GtkLabel *) label, buf);
}

static void ui_statusbar_playback_stop (void * unused, GtkWidget * label)
{
    gtk_label_set_text ((GtkLabel *) label, NULL);
}

static void ui_statusbar_destroy_cb (GtkWidget * widget, void * data)
{
    hook_dissociate ("playback ready", (HookFunction) ui_statusbar_info_change);
    hook_dissociate ("info change", (HookFunction) ui_statusbar_info_change);
    hook_dissociate ("playback stop", (HookFunction) ui_statusbar_playback_stop);
    hook_dissociate ("playlist activate", (HookFunction) ui_statusbar_update_playlist_length);
    hook_dissociate ("playlist update", (HookFunction) ui_statusbar_update_playlist_length);
}

GtkWidget * ui_statusbar_new (void)
{
    GtkWidget * hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
    GtkWidget * status = gtk_widget_new (GTK_TYPE_LABEL, "xalign", 0.0, NULL);

    gtk_label_set_ellipsize ((GtkLabel *) status, PANGO_ELLIPSIZE_END);
    gtk_box_pack_start ((GtkBox *) hbox, status, TRUE, TRUE, 5);

    hook_associate ("playback ready", (HookFunction) ui_statusbar_info_change, status);
    hook_associate ("info change", (HookFunction) ui_statusbar_info_change, status);
    hook_associate ("playback stop", (HookFunction) ui_statusbar_playback_stop, status);

    GtkWidget * length = gtk_widget_new (GTK_TYPE_LABEL, "xalign", 1.0, NULL);
    gtk_box_pack_start ((GtkBox *) hbox, length, FALSE, FALSE, 5);
    ui_statusbar_update_playlist_length (NULL, length);

    hook_associate ("playlist activate", (HookFunction) ui_statusbar_update_playlist_length, length);
    hook_associate ("playlist update", (HookFunction) ui_statusbar_update_playlist_length, length);

    g_signal_connect (hbox, "destroy", (GCallback) ui_statusbar_destroy_cb, NULL);

    if (aud_drct_get_playing () && aud_drct_get_ready ())
        ui_statusbar_info_change (NULL, status);

    return hbox;
}
