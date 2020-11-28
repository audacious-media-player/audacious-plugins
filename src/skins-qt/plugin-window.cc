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
#include <libaudqt/dock.h>
#include <libaudqt/libaudqt.h>

#include "main.h"
#include "window.h"

class PluginWindow : public QWidget
{
public:
    PluginWindow (audqt::DockItem * item) : m_item (item)
    {
        setWindowFlags (Qt::Dialog);
        setWindowTitle (item->name ());

        item->set_host_data (this);

        String pos_str = aud_get_str ("skins-layout", item->id ());
        int pos[4];

        if (pos_str && str_to_int_array (pos_str, pos, 4))
        {
            move (pos[0], pos[1]);
            resize (pos[2], pos[3]);
        }
        else
            resize (3 * audqt::sizes.OneInch, 2 * audqt::sizes.OneInch);

        auto vbox = audqt::make_vbox (this);
        vbox->addWidget (item->widget ());
    }

    QWidget * widget () const { return m_item->widget (); }

    void show ()
    {
        winId ();
        // FIXME: this breaks when double-size is enabled/disabled
        // (because the main window is recreated).
        windowHandle ()->setTransientParent (mainwin->windowHandle ());
        QWidget::show ();
    }

    void save_size ()
    {
        if (! isVisible ())
            return;

        int pos[4] = {x (), y (), width (), height ()};
        aud_set_str ("skins-layout", m_item->id (), int_array_to_str (pos, 4));
    }

    void destroy()
    {
        if (in_event)
            deleteLater();
        else
            delete this;
    }

protected:
    void closeEvent (QCloseEvent * event) override
    {
        in_event = true;
        m_item->user_close ();
        event->ignore ();
        in_event = false;
    }

    void keyPressEvent (QKeyEvent * event) override
    {
        auto mods = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier;
        if (! (event->modifiers () & mods) && event->key () == Qt::Key_Escape)
        {
            in_event = true;
            m_item->user_close ();
            event->accept ();
            in_event = false;
        }
    }

private:
    audqt::DockItem * m_item;
    bool in_event = false;
};

class PluginWindowHost : public audqt::DockHost
{
public:
    void add_dock_item(audqt::DockItem * item) override;
    void focus_dock_item(audqt::DockItem * item) override;
    void remove_dock_item(audqt::DockItem * item) override;
};

static PluginWindowHost window_host;
static Index<PluginWindow *> windows;

void PluginWindowHost::add_dock_item (audqt::DockItem * item)
{
    auto window = new PluginWindow (item);
    windows.append (window);

    if (aud_ui_is_shown ())
        window->show ();
}

void PluginWindowHost::focus_dock_item (audqt::DockItem * item)
{
    auto window = (PluginWindow *) item->host_data ();
    window->activateWindow ();
}

void PluginWindowHost::remove_dock_item (audqt::DockItem * item)
{
    auto window = (PluginWindow *) item->host_data ();
    windows.remove (windows.find (window), 1);
    window->save_size ();
    window->destroy ();
}

void create_plugin_windows ()
{
    audqt::register_dock_host (& window_host);
}

void show_plugin_windows ()
{
    for (auto window : windows)
        window->show ();
}

void focus_plugin_window (PluginHandle * plugin)
{
    auto item = audqt::DockItem::find_by_plugin (plugin);
    if (item)
        item->grab_focus ();
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
    audqt::unregister_dock_host ();
}
