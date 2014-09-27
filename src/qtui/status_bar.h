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

#include <QLabel>
#include <QStatusBar>

class StatusBar : public QStatusBar
{
public:
    StatusBar (QWidget * parent);
    ~StatusBar ();

private:
    QLabel * codec_label;
    QLabel * length_label;

    static void update_codec (void *, void * data);
    static void update_length (void *, void * data);
};

#endif // STATUS_BAR_H
