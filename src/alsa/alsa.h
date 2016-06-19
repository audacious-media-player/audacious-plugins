/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009-2014 John Lindgren
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

#ifndef AUDACIOUS_ALSA_H
#define AUDACIOUS_ALSA_H

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#define CHECK_VAL(value, function, ...) \
do { \
    (value) = function (__VA_ARGS__); \
    if ((value) < 0) { \
        AUDERR ("%s failed: %s.\n", #function, snd_strerror (value)); \
        goto FAILED; \
    } \
} while (0)

#define CHECK(function, ...) \
do { \
    int CHECK_error; \
    CHECK_VAL (CHECK_error, function, __VA_ARGS__); \
} while (0)

#define CHECK_STR(str, function, ...) \
do { \
    int CHECK_STR_error = function (__VA_ARGS__); \
    if (CHECK_STR_error < 0) { \
        str = String (str_printf ("ALSA error: %s failed: %s.\n", #function, \
         snd_strerror (CHECK_STR_error))); \
        goto FAILED; \
    } \
} while (0)

struct PreferencesWidget;

class ALSAPlugin : public OutputPlugin
{
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("ALSA Output"),
        PACKAGE,
        about,
        & prefs
    };

    constexpr ALSAPlugin () : OutputPlugin (info, 5) {}

    bool init ();
    void cleanup ();

    StereoVolume get_volume ();
    void set_volume (StereoVolume v);

    bool open_audio (int aud_format, int rate, int chans, String & error);
    void close_audio ();

    void period_wait ();
    int write_audio (const void * data, int size);
    void drain ();

    int get_delay ();

    void pause (bool pause);
    void flush ();

private:
    static void open_mixer ();
    static void close_mixer ();

    static void init_config ();
    static void pcm_changed ();
    static void mixer_changed ();
    static void element_changed ();
};

#endif
