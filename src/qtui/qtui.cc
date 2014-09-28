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

class QtUI : public audqt::QtIfacePlugin
{
private:
    int dummy_argc = 0;
    QApplication * qapp;
    MainWindow * window;

public:
    QtUI () : audqt::QtIfacePlugin ({N_("Qt Interface"), PACKAGE}) {}

    bool init ()
    {
        if (aud_get_mainloop_type () != MainloopType::Qt)
            return false;

        qapp = new QApplication (dummy_argc, 0);
        window = new MainWindow;

        return true;
    }

    void cleanup ()
    {
        delete window;
        window = nullptr;

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
