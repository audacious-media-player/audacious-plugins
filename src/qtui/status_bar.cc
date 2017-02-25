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
#include <libaudcore/i18n.h>
#include <libaudcore/mainloop.h>
#include <libaudcore/playlist.h>
#include <libaudcore/tinylock.h>

#include <QLabel>

#define TIMEOUT_MS 5000

static const char * normal_css =
 "QStatusBar { background: transparent; }\n"
 "QStatusBar::item { border: none; }";
static const char * warning_css =
 "QStatusBar { background: rgba(255,255,0,64); }\n"
 "QStatusBar::item { border: none; }";
static const char * error_css =
 "QStatusBar { background: rgba(255,0,0,64); }\n"
 "QStatusBar::item { border: none; }";

StatusBar::StatusBar (QWidget * parent) :
    QStatusBar (parent),
    codec_label (new QLabel (this)),
    length_label (new QLabel (this))
{
    addWidget (codec_label);
    addPermanentWidget (length_label);

    update_codec ();
    update_length ();

    setStyleSheet (normal_css);

    audlog::subscribe (log_handler, audlog::Warning);

    /* redisplay codec info when message is cleared */
    connect (this, & QStatusBar::messageChanged, [this] (const QString & text) {
        if (text.isEmpty ()) {
            setStyleSheet (normal_css);
            update_codec ();
        }
    });
}

StatusBar::~StatusBar ()
{
    audlog::unsubscribe (log_handler);
    event_queue_cancel ("qtui log message");
}

/* rate-limiting data */
static QueuedFunc message_func;
static TinyLock message_lock;
static int current_message_level = -1;
static unsigned current_message_serial = 0;

static void one_second_cb (void * serial)
{
    tiny_lock (& message_lock);

    /* allow new messages after one second */
    if (aud::from_ptr<unsigned> (serial) == current_message_serial)
        current_message_level = -1;

    tiny_unlock (& message_lock);
}

void StatusBar::log_handler (audlog::Level level, const char * file, int line,
 const char * func, const char * text)
{
    tiny_lock (& message_lock);

    /* do not replace a message of same or higher priority */
    if (level <= current_message_level)
    {
        tiny_unlock (& message_lock);
        return;
    }

    current_message_level = level;
    current_message_serial ++;

    message_func.queue (1000, one_second_cb, aud::to_ptr (current_message_serial));

    tiny_unlock (& message_lock);

    QString s = text;
    if (s.contains ('\n'))
        s = s.split ('\n', QString::SkipEmptyParts).last ();

    event_queue ("qtui log message", new Message {level, s}, aud::delete_obj<Message>);
}

void StatusBar::log_message (const Message * message)
{
    codec_label->hide ();
    setStyleSheet ((message->level == audlog::Error) ? error_css : warning_css);
    showMessage (message->text, TIMEOUT_MS);
}

void StatusBar::update_codec ()
{
    /* codec info is hidden when a message is displayed */
    if (! currentMessage ().isEmpty ())
        return;

    if (! aud_drct_get_ready ())
    {
        codec_label->hide ();
        return;
    }

    Tuple tuple = aud_drct_get_tuple ();
    String codec = tuple.get_str (Tuple::Codec);

    int bitrate, samplerate, channels;
    aud_drct_get_info (bitrate, samplerate, channels);

    StringBuf buf (0);

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

    codec_label->setText ((const char *) buf);
    codec_label->show ();
}

void StatusBar::update_length ()
{
    auto playlist = Playlist::active_playlist ();

    StringBuf s1 = str_format_time (playlist.selected_length_ms ());
    StringBuf s2 = str_format_time (playlist.total_length_ms ());

    length_label->setText ((const char *) str_concat ({s1, " / ", s2}));
}
