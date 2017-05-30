/**
 * Switches ReplayGain mode based on playback order.
 * Uses Album ReplayGain for Sequential or Shuffle by Album playback order.
 * Uses Track ReplayGain for Shuffle playback order.
 *
 * Copyright (C) 2016  kevinlekiller
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libaudcore/i18n.h>
#include <libaudcore/hook.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

class ReplayGainSwitch : public GeneralPlugin
{
public:
    static const char about[];
    static constexpr PluginInfo info = {
        N_("ReplayGain Switch"),
        PACKAGE,
        about
    };

    constexpr ReplayGainSwitch () : GeneralPlugin (info, true) {}

    bool init ();
    void cleanup ();
};

EXPORT ReplayGainSwitch aud_plugin_instance;

static void set_rg_mode(void *, void *) {
    /*
     * Use track gain if shuffle is on.
     * Use album gain if shuffle is off.
     * Use album gain if shuffle is on and album shuffle is on.
     */
    aud_set_bool(
        nullptr,
        "replay_gain_album",
        ! (aud_get_bool(nullptr, "shuffle") && ! aud_get_bool(nullptr, "album_shuffle"))
    );
}

bool ReplayGainSwitch::init () {
    // Set initially.
    set_rg_mode(nullptr, nullptr);
    // Check every time shuffle or album_shuffle is changed.
    hook_associate("set shuffle", set_rg_mode, nullptr);
    hook_associate("set album_shuffle", set_rg_mode, nullptr);
    return true;
}

void ReplayGainSwitch::cleanup () {
    hook_dissociate("set shuffle", set_rg_mode);
    hook_dissociate("set album_shuffle", set_rg_mode);
}

const char ReplayGainSwitch::about[] = N_(
    "ReplayGain Switch\n\n"
    "Switches ReplayGain mode based on playback order.\n"
    "Uses Album ReplayGain for Sequential or Shuffle by Album playback order.\n"
    "Uses Track ReplayGain for Shuffle playback order.\n\n"
    "Copyright (C) 2016  kevinlekiller\n\n"
    "This program is free software: you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public License as published by\n"
    "the Free Software Foundation, either version 3 of the License, or\n"
    "(at your option) any later version.\n\n"
    "This program is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "GNU General Public License for more details.\n\n"
    "You should have received a copy of the GNU General Public License\n"
    "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n"
);
