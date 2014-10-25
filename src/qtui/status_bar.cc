/*
 * status_bar.cc
 * Copyright 2014 John Lindgren
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

#include "status_bar.h"

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>

StatusBar::StatusBar (QWidget * parent) :
    QStatusBar (parent),
    codec_label (new QLabel (this)),
    length_label (new QLabel (this))
{
    setStyleSheet ("QStatusBar::item { border: none; }");

    addWidget (codec_label);
    addPermanentWidget (length_label);

    hook_associate ("playlist activate", update_length, this);
    hook_associate ("playlist update", update_length, this);

    hook_associate ("playback ready", update_codec, this);
    hook_associate ("playback stop", update_codec, this);
    hook_associate ("info change", update_codec, this);

    update_codec (nullptr, this);
    update_length (nullptr, this);
}

StatusBar::~StatusBar ()
{
    hook_dissociate_full ("playlist activate", update_length, this);
    hook_dissociate_full ("playlist update", update_length, this);

    hook_dissociate_full ("playback ready", update_codec, this);
    hook_dissociate_full ("playback stop", update_codec, this);
    hook_dissociate_full ("info change", update_codec, this);
}

void StatusBar::update_codec (void *, void * data)
{
    auto sb = (StatusBar *) data;

    if (! aud_drct_get_playing ())
    {
        sb->codec_label->hide ();
        return;
    }

    int playlist = aud_playlist_get_playing ();
    int position = aud_playlist_get_position (playlist);
    Tuple tuple = aud_playlist_entry_get_tuple (playlist, position);
    String codec = tuple.get_str (Tuple::Codec);

    int bitrate, samplerate, channels;
    aud_drct_get_info (& bitrate, & samplerate, & channels);

    StringBuf buf (0);

    if (codec)
    {
        str_insert (buf, -1, codec);
        if (channels > 0 || samplerate > 0 || bitrate > 0)
            str_insert (buf, -1, ", ");
    }

    if (channels > 0)
    {
        if (channels == 1)
            str_insert (buf, -1, _("mono"));
        else if (channels == 2)
            str_insert (buf, -1, _("stereo"));
        else
            buf.combine (str_printf (ngettext ("%d channel", "%d channels", channels), channels));

        if (samplerate > 0 || bitrate > 0)
            str_insert (buf, -1, ", ");
    }

    if (samplerate > 0)
    {
        buf.combine (str_printf ("%d kHz", samplerate / 1000));
        if (bitrate > 0)
            str_insert (buf, -1, ", ");
    }

    if (bitrate > 0)
        buf.combine (str_printf (_("%d kbps"), bitrate / 1000));

    sb->codec_label->setText ((const char *) buf);
    sb->codec_label->show ();
}

void StatusBar::update_length (void *, void * data)
{
    auto sb = (StatusBar *) data;

    int playlist = aud_playlist_get_active ();

    StringBuf s1 = str_format_time (aud_playlist_get_selected_length (playlist));
    StringBuf s2 = str_format_time (aud_playlist_get_total_length (playlist));

    sb->length_label->setText ((const char *) str_concat ({s1, " / ", s2}));
}
