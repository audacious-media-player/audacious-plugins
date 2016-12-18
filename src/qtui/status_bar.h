/*
 * status_bar.h
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

#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#include <QStatusBar>

#include <libaudcore/hook.h>
#include <libaudcore/runtime.h>

class QLabel;

class StatusBar : public QStatusBar
{
public:
    StatusBar (QWidget * parent);
    ~StatusBar ();

private:
    struct Message {
        audlog::Level level;
        QString text;
    };

    QLabel * codec_label;
    QLabel * length_label;

    static void log_handler (audlog::Level level, const char * file, int line,
     const char * func, const char * text);

    void log_message (const Message * message);

    void update_codec ();
    void update_length ();

    const HookReceiver<StatusBar, const Message *>
     log_hook {"qtui log message", this, & StatusBar::log_message};

    const HookReceiver<StatusBar>
     hook1 {"playlist activate", this, & StatusBar::update_length},
     hook2 {"playlist update", this, & StatusBar::update_length},
     hook3 {"playback ready", this, & StatusBar::update_codec},
     hook4 {"playback stop", this, & StatusBar::update_codec},
     hook5 {"info change", this, & StatusBar::update_codec},
     hook6 {"tuple change", this, & StatusBar::update_codec};
};

#endif
