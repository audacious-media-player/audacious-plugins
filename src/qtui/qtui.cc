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
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>
#include <libaudqt/iface.h>

#include "main_window.h"
#include "settings.h"

class QtUI : public audqt::QtIfacePlugin
{
private:
    MainWindow * window = nullptr;

public:
    constexpr QtUI () : audqt::QtIfacePlugin ({
        N_("Qt Interface"),
        PACKAGE,
        nullptr,
        & qtui_prefs,
        PluginQtOnly
    }) {}

    bool init ()
    {
        audqt::init ();
        aud_config_set_defaults ("qtui", qtui_defaults);
        window = new MainWindow;
        return true;
    }

    void cleanup ()
    {
        delete window;
        window = nullptr;
        audqt::cleanup ();
    }

    void run ()
    {
        audqt::run ();
    }

    void show (bool show)
    {
        window->setVisible (show);
    }

    void quit ()
    {
        audqt::quit ();
    }
};

EXPORT QtUI aud_plugin_instance;
