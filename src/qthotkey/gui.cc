/*
 *  This file is part of audacious-hotkey plugin for audacious
 *
 *  Copyright (C) 2020 i.Dark_Templar <darktemplar@dark-templar-archives.net>
 *  Copyright (c) 2007 - 2008  Sascha Hlusiak <contact@saschahlusiak.de>
 *  Name: gui.c
 *  Description: gui.c
 *
 *  Part of this code is from itouch-ctrl plugin.
 *  Authors of itouch-ctrl are listed below:
 *
 *  Copyright (c) 2006 - 2007 Vladimir Paskov <vlado.paskov@gmail.com>
 *
 *  Part of this code are from xmms-itouch plugin.
 *  Authors of xmms-itouch are listed below:
 *
 *  Copyright (C) 2000-2002 Ville Syrjälä <syrjala@sci.fi>
 *                         Bryn Davies <curious@ihug.com.au>
 *                         Jonathan A. Davis <davis@jdhouse.org>
 *                         Jeremy Tan <nsx@nsx.homeip.net>
 *
 *  audacious-hotkey is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  audacious-hotkey is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with audacious-hotkey; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#include "gui.h"
#include "plugin.h"

#include <QtCore/QMap>
#include <QtCore/QStringList>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtX11Extras/QX11Info>

#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>
#include <libaudcore/templates.h>

#include <libaudqt/libaudqt.h>

#include <X11/XKBlib.h>

namespace GlobalHotkeys
{

static PrefWidget * last_instance = nullptr;

static const QMap<Event, const char *> event_desc = {
    {Event::PrevTrack, N_("Previous track")},
    {Event::Play, N_("Play")},
    {Event::Pause, N_("Pause/Resume")},
    {Event::Stop, N_("Stop")},
    {Event::NextTrack, N_("Next track")},
    {Event::Forward, N_("Step forward")},
    {Event::Backward, N_("Step backward")},
    {Event::Mute, N_("Mute")},
    {Event::VolumeUp, N_("Volume up")},
    {Event::VolumeDown, N_("Volume down")},
    {Event::JumpToFile, N_("Jump to file")},
    {Event::ToggleWindow, N_("Toggle player window(s)")},
    {Event::ShowAOSD, N_("Show On-Screen-Display")},
    {Event::ToggleRepeat, N_("Toggle repeat")},
    {Event::ToggleShuffle, N_("Toggle shuffle")},
    {Event::ToggleStop, N_("Toggle stop after current")},
    {Event::Raise, N_("Raise player window(s)")},
};

class LineKeyEdit : public QLineEdit
{
public:
    explicit LineKeyEdit(QWidget * parent, HotkeyConfiguration & hotkey)
        : QLineEdit(parent), m_hotkey(hotkey)
    {
        set_keytext(0, 0);
    }

    void set_keytext(int key, int mask)
    {
        QString text;

        if ((key == 0) && (mask == 0))
        {
            text = audqt::translate_str("(none)");
        }
        else
        {
            static const char * modifier_string[] = {
                "Control", "Shift", "Alt", "Mod2", "Mod3", "Super", "Mod5"};
            static const unsigned int modifiers[] = {
                ControlMask, ShiftMask, Mod1Mask, Mod2Mask,
                Mod3Mask,    Mod4Mask,  Mod5Mask};

            QStringList strings;

            KeySym keysym;
            keysym = XkbKeycodeToKeysym(QX11Info::display(), key, 0, 0);
            if (keysym == 0 || keysym == NoSymbol)
            {
                text = QString::fromLocal8Bit("#%1").arg(key);
            }
            else
            {
                text = QString::fromLocal8Bit(XKeysymToString(keysym));
            }

            for (int j = 0; j < aud::n_elems(modifiers); ++j)
            {
                if (mask & modifiers[j])
                {
                    strings.push_back(QString::fromLatin1(modifier_string[j]));
                }
            }

            if (key != 0)
            {
                strings.push_back(text);
            }

            text = strings.join(QLatin1String(" + "));
        }

        setText(text);

        m_hotkey.key = key;
        m_hotkey.mask = mask;
    }

protected:
    void keyPressEvent(QKeyEvent * event) override
    {
        set_keytext(event->nativeScanCode(), event->nativeModifiers());
    }

private:
    HotkeyConfiguration & m_hotkey;
};

KeyControls::~KeyControls()
{
    delete combobox;
    delete keytext;
    delete button;
}

PrefWidget::PrefWidget(QWidget * parent)
    : QWidget(parent), main_widget_layout(new QVBoxLayout(this)),
      information_pixmap(new QLabel(this)),
      information_label(new QLabel(
          audqt::translate_str("Press a key combination inside a text field."),
          this)),
      information_layout(new QHBoxLayout()),
      group_box(new QGroupBox(audqt::translate_str("Hotkeys:"), this)),
      group_box_layout(new QGridLayout(group_box)),
      action_label(
          new QLabel(audqt::translate_str("<b>Action:</b>"), group_box)),
      key_binding_label(
          new QLabel(audqt::translate_str("<b>Key Binding:</b>"), group_box)),
      add_button(new QPushButton(audqt::get_icon("list-add"),
                                 audqt::translate_str("_Add"), this)),
      add_button_layout(new QHBoxLayout)
{
    int icon_size =
        QApplication::style()->pixelMetric(QStyle::PM_MessageBoxIconSize);
    information_pixmap->setPixmap(
        QApplication::style()
            ->standardIcon(QStyle::SP_MessageBoxInformation)
            .pixmap(QSize(icon_size, icon_size)));

    information_layout->addWidget(information_pixmap, 0, Qt::AlignLeft);
    information_layout->addWidget(information_label, 0, Qt::AlignLeft);
    information_layout->addStretch();

    action_label->setAlignment(Qt::AlignHCenter);
    key_binding_label->setAlignment(Qt::AlignHCenter);

    group_box->setLayout(group_box_layout);
    group_box_layout->addWidget(action_label, 0, 0);
    group_box_layout->addWidget(key_binding_label, 0, 1);

    for (const auto & hotkey : get_config()->hotkeys_list)
    {
        add_event_control(&hotkey);
    }

    add_button_layout->addWidget(add_button);
    add_button_layout->addStretch();

    this->setLayout(main_widget_layout);

    main_widget_layout->addLayout(information_layout);
    main_widget_layout->addWidget(group_box);
    main_widget_layout->addLayout(add_button_layout);

    QObject::connect(add_button, &QPushButton::clicked,
                     [this]() { add_event_control(nullptr); });

    last_instance = this;
}

PrefWidget::~PrefWidget()
{
    delete information_layout;

    for (auto control : controls_list)
    {
        delete control;
    }

    controls_list.clear();

    if (last_instance == this)
    {
        last_instance = nullptr;
    }
}

void PrefWidget::add_event_control(const HotkeyConfiguration * hotkey)
{
    KeyControls * control = new KeyControls;

    if (hotkey != nullptr)
    {
        control->hotkey.key = hotkey->key;
        control->hotkey.mask = hotkey->mask;
        control->hotkey.event = hotkey->event;

        if (control->hotkey.key == 0)
        {
            control->hotkey.mask = 0;
        }
    }
    else
    {
        control->hotkey.key = 0;
        control->hotkey.mask = 0;
        control->hotkey.event = static_cast<Event>(0);
    }

    control->combobox = new QComboBox(group_box);

    for (const auto & desc_item : event_desc)
    {
        control->combobox->addItem(audqt::translate_str(desc_item));
    }

    if (hotkey != nullptr)
    {
        control->combobox->setCurrentIndex(static_cast<int>(hotkey->event));
    }

    control->keytext = new LineKeyEdit(group_box, control->hotkey);
    control->keytext->setFocus(Qt::OtherFocusReason);

    if (hotkey != nullptr)
    {
        control->keytext->set_keytext(hotkey->key, hotkey->mask);
    }

    control->button = new QToolButton(group_box);
    control->button->setIcon(audqt::get_icon("edit-delete"));

    int row = group_box_layout->rowCount();

    controls_list.push_back(control);

    group_box_layout->addWidget(control->combobox, row, 0);
    group_box_layout->addWidget(control->keytext, row, 1);
    group_box_layout->addWidget(control->button, row, 2);

    QObject::connect(control->button, &QToolButton::clicked, [this, control]() {
        controls_list.removeAll(control);
        delete control;
    });
}

QList<HotkeyConfiguration> PrefWidget::getConfig() const
{
    QList<HotkeyConfiguration> result;

    for (const auto & control : controls_list)
    {
        HotkeyConfiguration hotkey;

        hotkey.key = control->hotkey.key;
        hotkey.mask = control->hotkey.mask;
        hotkey.event = static_cast<Event>(control->combobox->currentIndex());

        result.push_back(hotkey);
    }

    return result;
}

void * PrefWidget::make_config_widget()
{
    ungrab_keys();

    QWidget * main_widget = new PrefWidget;

    return main_widget;
}

void PrefWidget::ok_callback()
{
    if (last_instance != nullptr)
    {
        PluginConfig * plugin_cfg = get_config();
        plugin_cfg->hotkeys_list = last_instance->getConfig();
        save_config();
    }
}

void PrefWidget::destroy_callback() { grab_keys(); }

static const PreferencesWidget hotkey_widgets[] = {
    WidgetCustomQt(PrefWidget::make_config_widget)};

const PluginPreferences hotkey_prefs = {{hotkey_widgets},
                                        nullptr, // init
                                        PrefWidget::ok_callback,
                                        PrefWidget::destroy_callback};

} /* namespace GlobalHotkeys */
