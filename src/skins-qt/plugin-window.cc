/*
 * plugin-window.c
 * Copyright 2014 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

/* TODO (someday): implement proper docking for plugin windows */

#include "plugin-window.h"

#include <QBoxLayout>
#include <QWidget>
#include <QWindow>

#include <libaudcore/audstrings.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/hook.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

#include "main.h"
#include "window.h"

class PluginWindow : public QWidget
{
public:
    PluginWindow (PluginHandle * plugin, QWidget * widget) :
        m_plugin (plugin),
        m_widget (widget)
    {
        setWindowFlags (Qt::Dialog);
        setWindowTitle (aud_plugin_get_name (plugin));

        const char * basename = aud_plugin_get_basename (plugin);
        String pos_str = aud_get_str ("skins-layout", basename);
        int pos[4];

        if (pos_str && str_to_int_array (pos_str, pos, 4))
        {
            move (pos[0], pos[1]);
            resize (pos[2], pos[3]);
        }
        else
            resize (3 * audqt::sizes.OneInch, 2 * audqt::sizes.OneInch);

        auto vbox = audqt::make_vbox (this);
        vbox->addWidget (widget);
    }

    PluginHandle * plugin () const { return m_plugin; }
    QWidget * widget () const { return m_widget; }

    void show ()
    {
        winId ();
        windowHandle ()->setTransientParent (mainwin->windowHandle ());
        QWidget::show ();
    }

    void save_size ()
    {
        if (! isVisible ())
            return;

        const char * basename = aud_plugin_get_basename (m_plugin);
        int pos[4] = {x (), y (), width (), height ()};
        aud_set_str ("skins-layout", basename, int_array_to_str (pos, 4));
    }

protected:
    void closeEvent (QCloseEvent * event)
    {
        aud_plugin_enable (m_plugin, false);
        event->ignore ();
    }

    void keyPressEvent (QKeyEvent * event)
    {
        if (event->key () == Qt::Key_Escape)
        {
            aud_plugin_enable (m_plugin, false);
            event->accept ();
        }
    }

private:
    PluginHandle * m_plugin;
    QWidget * m_widget;
};

static Index<PluginWindow *> windows;

static void add_dock_plugin (PluginHandle * plugin, void *)
{
    auto widget = (QWidget *) aud_plugin_get_qt_widget (plugin);

    if (widget)
    {
        auto window = new PluginWindow (plugin, widget);
        windows.append (window);

        if (aud_ui_is_shown ())
            window->show ();
    }
}

static int find_dock_plugin (PluginHandle * plugin)
{
    int count = windows.len ();
    for (int i = 0; i < count; i ++)
    {
        if (windows[i]->plugin () == plugin)
            return i;
    }

    return -1;
}

static void remove_dock_plugin (PluginHandle * plugin, void * unused)
{
    int idx = find_dock_plugin (plugin);
    if (idx < 0)
        return;

    auto window = windows[idx];
    windows.remove (idx, 1);

    window->save_size ();
    delete window->widget ();
    window->deleteLater ();
}

void create_plugin_windows ()
{
    for (PluginHandle * plugin : aud_plugin_list (PluginType::General))
    {
        if (aud_plugin_get_enabled (plugin))
            add_dock_plugin (plugin, nullptr);
    }

    for (PluginHandle * plugin : aud_plugin_list (PluginType::Vis))
    {
        if (aud_plugin_get_enabled (plugin))
            add_dock_plugin (plugin, nullptr);
    }

    hook_associate ("dock plugin enabled", (HookFunction) add_dock_plugin, nullptr);
    hook_associate ("dock plugin disabled", (HookFunction) remove_dock_plugin, nullptr);
}

void show_plugin_windows ()
{
    for (auto window : windows)
        window->show ();
}

void focus_plugin_window (PluginHandle * plugin)
{
    int idx = find_dock_plugin (plugin);
    if (idx >= 0)
        windows[idx]->activateWindow ();

    aud_plugin_send_message (plugin, "grab focus", nullptr, 0);
}

void hide_plugin_windows ()
{
    for (auto window : windows)
    {
        window->save_size ();
        window->hide ();
    }
}

void destroy_plugin_windows ()
{
    for (PluginHandle * plugin : aud_plugin_list (PluginType::General))
    {
        if (aud_plugin_get_enabled (plugin))
            remove_dock_plugin (plugin, nullptr);
    }

    for (PluginHandle * plugin : aud_plugin_list (PluginType::Vis))
    {
        if (aud_plugin_get_enabled (plugin))
            remove_dock_plugin (plugin, nullptr);
    }

    hook_dissociate ("dock plugin enabled", (HookFunction) add_dock_plugin);
    hook_dissociate ("dock plugin disabled", (HookFunction) remove_dock_plugin);
}
