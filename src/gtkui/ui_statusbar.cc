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

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/mainloop.h>
#include <libaudcore/playlist.h>
#include <libaudcore/runtime.h>

#include "ui_statusbar.h"

static QueuedFunc clear_timeout;

static void ui_statusbar_update_playlist_length (void *, void * label)
{
    auto playlist = Playlist::active_playlist ();
    StringBuf s1 = str_format_time (playlist.selected_length_ms ());
    StringBuf s2 = str_format_time (playlist.total_length_ms ());
    gtk_label_set_text ((GtkLabel *) label, str_concat ({s1, " / ", s2}));
}

static void ui_statusbar_info_change (void *, void * label)
{
    if (clear_timeout.running ())
        return;

    Tuple tuple = aud_drct_get_tuple ();
    String codec = tuple.get_str (Tuple::Codec);

    int bitrate, samplerate, channels;
    aud_drct_get_info (bitrate, samplerate, channels);

    StringBuf buf;

    if (codec)
    {
        buf.insert (-1, codec);
        if (channels > 0 || samplerate > 0 || bitrate > 0)
            buf.insert (-1, ", ");
    }

    if (channels > 0)
    {
        if (channels == 1)
            buf.insert (-1, _("mono"));
        else if (channels == 2)
            buf.insert (-1, _("stereo"));
        else
            buf.combine (str_printf (ngettext ("%d channel", "%d channels", channels), channels));

        if (samplerate > 0 || bitrate > 0)
            buf.insert (-1, ", ");
    }

    if (samplerate > 0)
    {
        buf.combine (str_printf (_("%d kHz"), samplerate / 1000));
        if (bitrate > 0)
            buf.insert (-1, ", ");
    }

    if (bitrate > 0)
        buf.combine (str_printf (_("%d kbps"), bitrate / 1000));

    gtk_label_set_text ((GtkLabel *) label, buf);
}

static void ui_statusbar_playback_stop (void *, void * label)
{
    if (clear_timeout.running ())
        return;

    gtk_label_set_text ((GtkLabel *) label, nullptr);
}

static void clear_message (void * label)
{
    clear_timeout.stop ();

    if (aud_drct_get_ready ())
        ui_statusbar_info_change (nullptr, label);
    else
        gtk_label_set_text ((GtkLabel *) label, nullptr);
}

static void no_advance_toggled (void *, void * label)
{
    if (aud_get_bool (nullptr, "no_playlist_advance"))
        gtk_label_set_text ((GtkLabel *) label, _("Single mode."));
    else
        gtk_label_set_text ((GtkLabel *) label, _("Playlist mode."));

    clear_timeout.start (1000, clear_message, label);
}

static void stop_after_song_toggled (void *, void * label)
{
    if (aud_get_bool (nullptr, "stop_after_current_song"))
        gtk_label_set_text ((GtkLabel *) label, _("Stopping after song."));

    clear_timeout.start (1000, clear_message, label);
}

static void ui_statusbar_destroy_cb ()
{
    clear_timeout.stop ();

    hook_dissociate ("playback ready", ui_statusbar_info_change);
    hook_dissociate ("info change", ui_statusbar_info_change);
    hook_dissociate ("tuple change", ui_statusbar_info_change);
    hook_dissociate ("playback stop", ui_statusbar_playback_stop);
    hook_dissociate ("set no_playlist_advance", no_advance_toggled);
    hook_dissociate ("set stop_after_current_song", stop_after_song_toggled);
    hook_dissociate ("playlist activate", ui_statusbar_update_playlist_length);
    hook_dissociate ("playlist update", ui_statusbar_update_playlist_length);
}

GtkWidget * ui_statusbar_new ()
{
    GtkWidget * hbox = gtk_hbox_new (false, 3);
    GtkWidget * status = gtk_widget_new (GTK_TYPE_LABEL, "xalign", 0.0, nullptr);
    GtkWidget * length = gtk_widget_new (GTK_TYPE_LABEL, "xalign", 1.0, nullptr);

    gtk_label_set_ellipsize ((GtkLabel *) status, PANGO_ELLIPSIZE_END);
    gtk_box_pack_start ((GtkBox *) hbox, status, true, true, 5);
    gtk_box_pack_start ((GtkBox *) hbox, length, false, false, 5);

    ui_statusbar_update_playlist_length (nullptr, length);

    hook_associate ("playback ready", ui_statusbar_info_change, status);
    hook_associate ("info change", ui_statusbar_info_change, status);
    hook_associate ("tuple change", ui_statusbar_info_change, status);
    hook_associate ("playback stop", ui_statusbar_playback_stop, status);
    hook_associate ("set no_playlist_advance", no_advance_toggled, status);
    hook_associate ("set stop_after_current_song", stop_after_song_toggled, status);
    hook_associate ("playlist activate", ui_statusbar_update_playlist_length, length);
    hook_associate ("playlist update", ui_statusbar_update_playlist_length, length);

    g_signal_connect (hbox, "destroy", (GCallback) ui_statusbar_destroy_cb, nullptr);

    if (aud_drct_get_ready ())
        ui_statusbar_info_change (nullptr, status);

    return hbox;
}
