/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2008 Tomasz Moń
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */


#include "skins_cfg.h"
#include "ui_skin.h"
#include <glib.h>
#include <stdlib.h>
#include <audacious/plugin.h>

skins_cfg_t * skins_cfg_new(void) {
    skins_cfg_t *cfg = g_malloc0(sizeof(skins_cfg_t));
    cfg->set = FALSE;
    cfg->skin = NULL;
    return cfg;
}


void skins_cfg_delete(skins_cfg_t * cfg) {
  if (cfg != NULL) {
      if (cfg->skin) g_free(cfg->skin);
      g_free(cfg);
  }
}

gint skins_cfg_load(skins_cfg_t * cfg) {
  mcs_handle_t *cfgfile = aud_cfg_db_open();

  /* if (!aud_cfg_db_get_int(cfgfile, "skins", "field_name", &(cfg->where)))
         cfg->where = default value
     if (!aud_cfg_db_get_string(cfgfile, "skins", "field_name", &(cfg->where)))
         cfg->where = g_strdup("defaul");
     if (!aud_cfg_db_get_bool(cfgfile, "skins", "field_name", &(cfg->where)))
         cfg->where = FALSE / TRUE;
  */

  if (!aud_cfg_db_get_string(cfgfile, "skins", "skin", &(cfg->skin)))
      cfg->skin = g_strdup(BMP_DEFAULT_SKIN_PATH);

  if (!aud_cfg_db_get_bool(cfgfile, "skins", "scaled", &(cfg->scaled)))
      cfg->scaled = FALSE;

  if (!aud_cfg_db_get_bool(cfgfile, "skins", "always_on_top", &(cfg->always_on_top)))
      cfg->always_on_top = FALSE;

  if (!aud_cfg_db_get_float(cfgfile, "skins", "scale_factor", &(cfg->scale_factor)))
      cfg->scale_factor = 2.0;

  if (!aud_cfg_db_get_bool(cfgfile, "skins", "always_show_cb", &(cfg->always_show_cb)))
      cfg->always_show_cb = TRUE;

  aud_cfg_db_close( cfgfile );

  cfg->set = TRUE;

  return 0;
}


gint skins_cfg_save(skins_cfg_t * cfg) {
    mcs_handle_t *cfgfile = aud_cfg_db_open();

    if (cfg->set == FALSE)
        return -1;

/*
    aud_cfg_db_set_int(cfgfile, "skins", "field_name", cfg->where);
    aud_cfg_db_set_string(cfgfile, "skins", "field_name", cfg->where);
    aud_cfg_db_set_bool(cfgfile, "skins", "field_name", cfg->where);
*/
    aud_cfg_db_set_string(cfgfile, "skins", "skin", cfg->skin);

    aud_cfg_db_close(cfgfile);

    return 0;
}
