/* 
 *  XMMS Crossfade Plugin
 *  Copyright (C) 2000-2007  Peter Eisenlohr <peter@eisenlohr.org>
 *
 *  based on the original OSS Output Plugin
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

#ifndef _CFGUTIL_H_
#define _CFGUTIL_H_

#include "crossfade.h"

/* configuration load/save functions */
void xfade_load_config();
void xfade_save_config();
void xfade_free_config();

void xfade_load_plugin_config(gchar  *config_string, gchar *plugin_name, plugin_config_t *plugin_config);
void xfade_save_plugin_config(gchar **config_string, gchar *plugin_name, plugin_config_t *plugin_confg);

/* some helper functions */
gint xfade_mix_size_ms(config_t * cfg);

gint xfade_cfg_out_skip      (fade_config_t *fc);
gint xfade_cfg_fadeout_len   (fade_config_t *fc);
gint xfade_cfg_fadeout_volume(fade_config_t *fc);
gint xfade_cfg_offset        (fade_config_t *fc);
gint xfade_cfg_in_skip       (fade_config_t *fc);
gint xfade_cfg_fadein_len    (fade_config_t *fc);
gint xfade_cfg_fadein_volume (fade_config_t *fc);

gboolean xfade_cfg_gap_trail_enable(config_t *cfg);
gint     xfade_cfg_gap_trail_len   (config_t *cfg);
gint     xfade_cfg_gap_trail_level (config_t *cfg);

#endif /* _CFGUTIL_H_ */

