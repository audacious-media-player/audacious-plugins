#include "config.h"

#include "vorbis.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include <audacious/configdb.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>
#include <audacious/i18n.h>

vorbis_config_t vorbis_cfg;

static void
vorbis_save_config()
{
    mcs_handle_t *db;

    db = aud_cfg_db_open();
    aud_cfg_db_set_bool(db, "vorbis", "tag_override",
                        vorbis_cfg.tag_override);
    aud_cfg_db_set_string(db, "vorbis", "tag_format", vorbis_cfg.tag_format);
    aud_cfg_db_close(db);
}

vorbis_config_t config; /* temporary config */

static PreferencesWidget settings_elements[] = {
    {WIDGET_CHK_BTN, N_("Override generic titles"), &config.tag_override, NULL, NULL, FALSE},
    {WIDGET_ENTRY, N_("Title format:"), &config.tag_format, NULL, NULL, TRUE, {.entry = {FALSE}}, VALUE_STRING},
};

static PreferencesWidget prefs[] = {
    {WIDGET_BOX, N_("Ogg Vorbis Tags"), NULL, NULL, NULL, FALSE,
        {.box = {settings_elements,
                 G_N_ELEMENTS(settings_elements),
                 FALSE /* vertical */, TRUE /* frame */}}},
};

static void
configure_apply()
{
    vorbis_cfg.tag_override = config.tag_override;

    g_free(vorbis_cfg.tag_format);
    vorbis_cfg.tag_format = g_strdup(config.tag_format);

    vorbis_save_config();
}

static void
configure_init(void)
{
    memcpy(&config, &vorbis_cfg, sizeof(config));
    config.tag_format = g_strdup(vorbis_cfg.tag_format);
}

static void
configure_cleanup(void)
{
    g_free(config.tag_format);
    config.tag_format = NULL;
}

PluginPreferences preferences = {
    .title = N_("Ogg Vorbis Audio Plugin Configuration"),
    .prefs = prefs,
    .n_prefs = G_N_ELEMENTS(prefs),
    .type = PREFERENCES_WINDOW,
    .init = configure_init,
    .apply = configure_apply,
    .cleanup = configure_cleanup,
};
