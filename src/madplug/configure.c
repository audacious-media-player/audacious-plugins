/*
 * mad plugin for audacious
 * Copyright (C) 2009 Tomasz Moń
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
 *
 * Portions derived from xmms-mad:
 * Copyright (C) 2001-2002 Sam Clegg - See COPYING
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* #define AUD_DEBUG 1 */

#include "plugin.h"

#include <audacious/preferences.h>
#include <math.h>

static audmad_config_t config;

static PreferencesWidget metadata_settings_elements[] = {
    {WIDGET_CHK_BTN, N_("Enable fast play-length calculation"), &config.fast_play_time_calc, NULL, NULL, FALSE},
    {WIDGET_CHK_BTN, N_("Parse XING headers"), &config.use_xing, NULL, NULL, FALSE},
    {WIDGET_CHK_BTN, N_("Use SJIS to write ID3 tags (not recommended)"), &config.sjis, NULL, NULL, FALSE},
};

static PreferencesWidget metadata_settings[] = {
    {WIDGET_BOX, N_("Metadata Settings"), NULL, NULL, NULL, FALSE,
        {.box = {metadata_settings_elements,
                 G_N_ELEMENTS(metadata_settings_elements),
                 FALSE /* vertical */, TRUE /* frame */}}},
};

static void
update_config()
{
    memcpy(audmad_config, &config, sizeof(config));
}

static void
save_config(void)
{
    mcs_handle_t *db = aud_cfg_db_open();

    //metadata
    aud_cfg_db_set_bool(db, "MAD", "fast_play_time_calc",
                        audmad_config->fast_play_time_calc);
    aud_cfg_db_set_bool(db, "MAD", "use_xing", audmad_config->use_xing);
    aud_cfg_db_set_bool(db, "MAD", "sjis", audmad_config->sjis);

    aud_cfg_db_close(db);
}

static void
configure_apply()
{
    update_config();
    save_config();
}

static void
configure_init(void)
{
    memcpy(&config, audmad_config, sizeof(config));
}

static NotebookTab preferences_tabs[] = {
    {N_("General"), metadata_settings, G_N_ELEMENTS(metadata_settings)},
};

static PreferencesWidget prefs[] = {
    {WIDGET_NOTEBOOK, NULL, NULL, NULL, NULL, FALSE,
        {.notebook = {preferences_tabs, G_N_ELEMENTS(preferences_tabs)}}},
};

PluginPreferences preferences = {
    .title = N_("MPEG Audio Plugin Configuration"),
    .prefs = prefs,
    .n_prefs = G_N_ELEMENTS(prefs),
    .type = PREFERENCES_WINDOW,
    .init = configure_init,
    .apply = configure_apply,
};
