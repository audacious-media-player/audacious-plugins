/*
 * moonstone.cc
 * Copyright 2020 Ariadne Conill
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

#include <libaudqt/iface.h>
#include <libaudqt/libaudqt.h>

#include "main_window.h"

class MoonstoneUI : public audqt::QtIfacePlugin
{
private:
    Moonstone::MainWindow *m_window = nullptr;

public:
    constexpr MoonstoneUI()
        : audqt::QtIfacePlugin(
              {N_("Moonstone"), PACKAGE, nullptr, nullptr, PluginQtOnly})
    {
    }

    bool init()
    {
        audqt::init();
//        aud_config_set_defaults("moonstoneui", moonstoneui_defaults);
        m_window = new Moonstone::MainWindow;
        return true;
    }

    void cleanup()
    {
        delete m_window;
        m_window = nullptr;

        audqt::cleanup();
    }

    void run() { audqt::run(); }

    void show(bool show)
    {
        m_window->setVisible(show);

        if (show)
        {
            m_window->activateWindow();
            m_window->raise();
        }
    }

    void quit() { audqt::quit(); }
};

EXPORT MoonstoneUI aud_plugin_instance;
