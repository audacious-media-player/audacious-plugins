/*
 * qtui.cc
 * Copyright 2014 Michał Lipski
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

#include <QApplication>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>

#include "main_window.h"

static int dummy_argc = 0;
static QApplication * qapp;
static MainWindow * window;

static bool init ()
{
    if (aud_get_mainloop_type () != MainloopType::Qt)
        return false;

    qapp = new QApplication (dummy_argc, 0);
    window = new MainWindow;

    return true;
}

static void cleanup ()
{
    delete window;
    window = nullptr;

    delete qapp;
    qapp = nullptr;
}

static void run ()
{
    qapp->exec ();
}

static void show (bool show)
{
    window->setVisible (show);
}

static void quit ()
{
    qapp->quit();
}

#define AUD_PLUGIN_NAME     N_("Qt Interface")
#define AUD_PLUGIN_INIT     init
#define AUD_PLUGIN_CLEANUP  cleanup
#define AUD_IFACE_RUN       run

#define AUD_IFACE_SHOW      show
#define AUD_IFACE_QUIT      quit

#define AUD_IFACE_SHOW_ABOUT         audqt::aboutwindow_show
#define AUD_IFACE_HIDE_ABOUT         audqt::aboutwindow_hide

#define AUD_IFACE_SHOW_SETTINGS      audqt::prefswin_show
#define AUD_IFACE_HIDE_SETTINGS      audqt::prefswin_hide

#define AUD_DECLARE_IFACE
#include <libaudcore/plugin-declare.h>
