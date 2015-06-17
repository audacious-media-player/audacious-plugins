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

#define AUD_PLUGIN_QT_ONLY
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>
#include <libaudqt/iface.h>

#include "main_window.h"

static char app_name[] = "audacious";
static int dummy_argc = 1;
static char * dummy_argv[] = {app_name, nullptr};

class QtUI : public audqt::QtIfacePlugin
{
private:
    QApplication * qapp = nullptr;
    MainWindow * window = nullptr;

public:
    constexpr QtUI () : audqt::QtIfacePlugin ({N_("Qt Interface"), PACKAGE}) {}

    bool init ()
    {
        qapp = new QApplication (dummy_argc, dummy_argv);
        qapp->setAttribute(Qt::AA_UseHighDpiPixmaps);
        window = new MainWindow;

        return true;
    }

    void cleanup ()
    {
        delete window;
        window = nullptr;

        audqt::cleanup ();

        delete qapp;
        qapp = nullptr;
    }

    void run ()
    {
        qapp->exec ();
    }

    void show (bool show)
    {
        window->setVisible (show);
    }

    void quit ()
    {
        qapp->quit();
    }
};

EXPORT QtUI aud_plugin_instance;
