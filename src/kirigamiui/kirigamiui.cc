/*
 * Copyright 2026 Audacious developers and others
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
 */

#include <memory>

#include <QApplication>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudqt/iface.h>
#include <libaudqt/libaudqt.h>

#include "mobile_ui.h"

static std::unique_ptr<QQmlApplicationEngine> engine;
static std::unique_ptr<MobileUiController> controller;
static QPointer<QQuickWindow> window;

class KirigamiUi : public audqt::QtIfacePlugin
{
public:
    constexpr KirigamiUi()
        : audqt::QtIfacePlugin({N_("Kirigami Mobile Interface"), PACKAGE,
                                nullptr, nullptr, PluginQtOnly})
    {
    }

    bool init() override
    {
        audqt::init();

        controller = std::make_unique<MobileUiController>();
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->rootContext()->setContextProperty("player", controller.get());
        engine->load(QUrl(QStringLiteral(
            "qrc:/org/audacious/kirigamiui/Main.qml")));

        if (engine->rootObjects().isEmpty())
        {
            engine.reset();
            controller.reset();
            audqt::cleanup();
            return false;
        }

        window = qobject_cast<QQuickWindow *>(engine->rootObjects().constFirst());
        if (!window)
        {
            engine.reset();
            controller.reset();
            audqt::cleanup();
            return false;
        }

        return true;
    }

    void cleanup() override
    {
        window.clear();
        engine.reset();
        controller.reset();
        audqt::cleanup();
    }

    void run() override { QApplication::exec(); }

    void show_about_window() override
    {
        show(true);
        if (controller)
            controller->showAbout();
    }

    void hide_about_window() override
    {
        if (controller)
            controller->hideAbout();
    }

    void show_prefs_window() override
    {
        show(true);
        if (controller)
            controller->showPreferences();
    }

    void hide_prefs_window() override
    {
        if (controller)
            controller->hidePreferences();
    }

    void show(bool show) override
    {
        if (!window)
            return;

        window->setVisible(show);
        if (show)
        {
            window->raise();
            window->requestActivate();
        }
    }

    void quit() override
    {
        if (!window)
        {
            QApplication::quit();
            return;
        }

        QObject::connect(window.data(), &QObject::destroyed,
                         QApplication::quit);
        window->deleteLater();
    }
};

EXPORT KirigamiUi aud_plugin_instance;
