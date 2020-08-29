/*
 *  This file is part of audacious-hotkey plugin for audacious
 *
 *  Copyright (c) 2007 - 2008  Sascha Hlusiak <contact@saschahlusiak.de>
 *  Name: plugin.c
 *  Description: plugin.c
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

#include <gtk/gtk.h>

#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#include "api_hotkey.h"
#include "grab.h"
#include "gui.h"
#include "plugin.h"

#ifdef BUILT_FROM_CMAKE
#include "../../audacious-plugins_simpleAF/src/thirdparty/d_custom_logger.hpp"
#endif

class GlobalHotkeys : public GeneralPlugin
{
public:
    static const char about[];

    static constexpr PluginInfo info = {N_("Global Hotkeys"), PACKAGE, about,
                                        &hotkey_prefs, PluginGLibOnly};

    constexpr GlobalHotkeys() : GeneralPlugin(info, false) {}

    bool init() override;
    void cleanup() override;
};

EXPORT GlobalHotkeys aud_plugin_instance;

#ifndef _WIN32
/* global vars */
static
#else
PluginConfig plugin_cfg;
#endif

    const char GlobalHotkeys::about[] = N_(
        "Global Hotkey Plugin\n"
        "Control the player with global key combinations or multimedia "
        "keys.\n\n"
        "Copyright (C) 2007-2008 Sascha Hlusiak <contact@saschahlusiak.de>\n\n"
        "Contributors include:\n"
        "Copyright (C) 2006-2007 Vladimir Paskov <vlado.paskov@gmail.com>\n"
        "Copyright (C) 2000-2002 Ville Syrjälä <syrjala@sci.fi>,\n"
        " Bryn Davies <curious@ihug.com.au>,\n"
        " Jonathan A. Davis <davis@jdhouse.org>,\n"
        " Jeremy Tan <nsx@nsx.homeip.net>");

PluginConfig * get_config() { return &plugin_cfg; }

/*
 * plugin activated
 */
bool GlobalHotkeys::init()
{
#ifdef BUILT_FROM_CMAKE
    audlog::subscribe(&DCustomLogger::go, audlog::Level::Debug);
#endif
    if (!gtk_init_check(nullptr, nullptr))
    {
        AUDERR("GTK+ initialization failed.\n");
        return false;
    }
#ifdef _WIN32
    win_init();
#endif
    setup_filter();
    load_config();
    grab_keys();
    return true;
}

/* handle keys */
gboolean handle_keyevent(EVENT event)
{
    int current_volume, old_volume;
    static int volume_static = 0;
    gboolean mute;

    /* get current volume */
    current_volume = aud_drct_get_volume_main();
    old_volume = current_volume;
    if (current_volume)
    {
        /* volume is not mute */
        mute = false;
    }
    else
    {
        /* volume is mute */
        mute = true;
    }

    /* mute the playback */
    if (event == EVENT_MUTE)
    {
        if (!mute)
        {
            volume_static = current_volume;
            aud_drct_set_volume_main(0);
            mute = true;
        }
        else
        {
            aud_drct_set_volume_main(volume_static);
            mute = false;
        }
        return true;
    }

    /* decrease volume */
    if (event == EVENT_VOL_DOWN)
    {
        if (mute)
        {
            current_volume = old_volume;
            old_volume = 0;
            mute = false;
        }

        if ((current_volume -= aud_get_int("volume_delta")) < 0)
        {
            current_volume = 0;
        }

        if (current_volume != old_volume)
        {
            aud_drct_set_volume_main(current_volume);
        }

        old_volume = current_volume;
        return true;
    }

    /* increase volume */
    if (event == EVENT_VOL_UP)
    {
        if (mute)
        {
            current_volume = old_volume;
            old_volume = 0;
            mute = false;
        }

        if ((current_volume += aud_get_int("volume_delta")) > 100)
        {
            current_volume = 100;
        }

        if (current_volume != old_volume)
        {
            aud_drct_set_volume_main(current_volume);
        }

        old_volume = current_volume;
        return true;
    }

    /* play */
    if (event == EVENT_PLAY)
    {
        aud_drct_play();
        return true;
    }

    /* pause */
    if (event == EVENT_PAUSE)
    {
        aud_drct_play_pause();
        return true;
    }

    /* stop */
    if (event == EVENT_STOP)
    {
        aud_drct_stop();
        return true;
    }

    /* prev track */
    if (event == EVENT_PREV_TRACK)
    {
        aud_drct_pl_prev();
        return true;
    }

    /* next track */
    if (event == EVENT_NEXT_TRACK)
    {
        aud_drct_pl_next();
        return true;
    }

    /* forward */
    if (event == EVENT_FORWARD)
    {
        aud_drct_seek(aud_drct_get_time() + aud_get_int("step_size") * 1000);
        return true;
    }

    /* backward */
    if (event == EVENT_BACKWARD)
    {
        aud_drct_seek(aud_drct_get_time() - aud_get_int("step_size") * 1000);
        return true;
    }

    /* Open Jump-To-File dialog */
    if (event == EVENT_JUMP_TO_FILE && !aud_get_headless_mode())
    {
        aud_ui_show_jump_to_song();
        return true;
    }

    /* Toggle Windows */
    if (event == EVENT_TOGGLE_WIN && !aud_get_headless_mode())
    {
        aud_ui_show(!aud_ui_is_shown());
        return true;
    }

    /* Show OSD through AOSD plugin*/
    if (event == EVENT_SHOW_AOSD)
    {
        hook_call("aosd toggle", nullptr);
        return true;
    }

    if (event == EVENT_TOGGLE_REPEAT)
    {
        aud_toggle_bool("repeat");
        return true;
    }

    if (event == EVENT_TOGGLE_SHUFFLE)
    {
        aud_toggle_bool("shuffle");
        return true;
    }

    if (event == EVENT_TOGGLE_STOP)
    {
        aud_toggle_bool("stop_after_current_song");
        return true;
    }

    if (event == EVENT_RAISE)
    {
        aud_ui_show(true);
        return true;
    }

    return false;
}

void load_defaults()
{
    AUDDBG("lHotkeyFlow:Entry, loading defaults.");
    HotkeyConfiguration * hotkey;

    hotkey = &(plugin_cfg.first);

    Hotkey::add_hotkey(&hotkey, OS_KEY_AudioPrev, 0, TYPE_KEY,
                       EVENT_PREV_TRACK);
    Hotkey::add_hotkey(&hotkey, OS_KEY_AudioPlay, 0, TYPE_KEY, EVENT_PLAY);
    Hotkey::add_hotkey(&hotkey, OS_KEY_AudioPause, 0, TYPE_KEY, EVENT_PAUSE);
    Hotkey::add_hotkey(&hotkey, OS_KEY_AudioStop, 0, TYPE_KEY, EVENT_STOP);
    Hotkey::add_hotkey(&hotkey, OS_KEY_AudioNext, 0, TYPE_KEY,
                       EVENT_NEXT_TRACK);

    /*    add_hotkey(&hotkey, OS_KEY_AudioRewind, 0, TYPE_KEY, EVENT_BACKWARD);
     */

    Hotkey::add_hotkey(&hotkey, OS_KEY_AudioMute, 0, TYPE_KEY, EVENT_MUTE);
    Hotkey::add_hotkey(&hotkey, OS_KEY_AudioRaiseVolume, 0, TYPE_KEY,
                       EVENT_VOL_UP);
    Hotkey::add_hotkey(&hotkey, OS_KEY_AudioLowerVolume, 0, TYPE_KEY,
                       EVENT_VOL_DOWN);

    /*    add_hotkey(&hotkey, OS_KEY_AudioMedia, 0, TYPE_KEY,
       EVENT_JUMP_TO_FILE); add_hotkey(&hotkey, XF86XK_Music, 0, TYPE_KEY,
       EVENT_TOGGLE_WIN); */
}

/* load plugin configuration */
void load_config()
{
    HotkeyConfiguration * hotkey;
    int i, max;

    hotkey = &(plugin_cfg.first);
    hotkey->next = nullptr;
    hotkey->key = 0;
    hotkey->mask = 0;
    hotkey->event = (EVENT)0;
    hotkey->type = TYPE_KEY;

    max = aud_get_int("globalHotkey", "NumHotkeys");
    if (max == 0)
        load_defaults();
    else
        for (i = 0; i < max; i++)
        {
            char * text = nullptr;

            if (hotkey->key)
            {
                hotkey->next = g_new(HotkeyConfiguration, 1);
                hotkey = hotkey->next;
                hotkey->next = nullptr;
                hotkey->key = 0;
                hotkey->mask = 0;
                hotkey->event = (EVENT)0;
                hotkey->type = TYPE_KEY;
            }
            text = g_strdup_printf("Hotkey_%d_key", i);
            hotkey->key = aud_get_int("globalHotkey", text);
            g_free(text);

            text = g_strdup_printf("Hotkey_%d_mask", i);
            hotkey->mask = aud_get_int("globalHotkey", text);
            g_free(text);

            text = g_strdup_printf("Hotkey_%d_type", i);
            hotkey->type = aud_get_int("globalHotkey", text);
            g_free(text);

            text = g_strdup_printf("Hotkey_%d_event", i);
            hotkey->event = (EVENT)aud_get_int("globalHotkey", text);
            g_free(text);
        }
}

/* save plugin configuration */
void save_config()
{
    int max;
    HotkeyConfiguration * hotkey;

    hotkey = &(plugin_cfg.first);
    max = 0;
    while (hotkey)
    {
        char * text = nullptr;
        if (hotkey->key)
        {
            text = g_strdup_printf("Hotkey_%d_key", max);
            aud_set_int("globalHotkey", text, hotkey->key);
            g_free(text);

            text = g_strdup_printf("Hotkey_%d_mask", max);
            aud_set_int("globalHotkey", text, hotkey->mask);
            g_free(text);

            text = g_strdup_printf("Hotkey_%d_type", max);
            aud_set_int("globalHotkey", text, hotkey->type);
            g_free(text);

            text = g_strdup_printf("Hotkey_%d_event", max);
            aud_set_int("globalHotkey", text, hotkey->event);
            g_free(text);
            max++;
        }

        hotkey = hotkey->next;
    }

    aud_set_int("globalHotkey", "NumHotkeys", max);
}

void GlobalHotkeys::cleanup()
{
#ifdef BUILT_FROM_CMAKE
    audlog::unsubscribe(&DCustomLogger::go);
#endif

    HotkeyConfiguration * hotkey;
    ungrab_keys();
    release_filter();
    hotkey = &(plugin_cfg.first);
    hotkey = hotkey->next;
    while (hotkey)
    {
        HotkeyConfiguration * old;
        old = hotkey;
        hotkey = hotkey->next;
        g_free(old);
    }
    plugin_cfg.first.next = nullptr;
    plugin_cfg.first.key = 0;
    plugin_cfg.first.event = (EVENT)0;
    plugin_cfg.first.mask = 0;
}
