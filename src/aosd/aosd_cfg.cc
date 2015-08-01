/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2007
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/

#include "aosd_cfg.h"
#include "aosd_style.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>

static aosd_color_t str_to_color (const char * str)
{
  /* color strings are in format "x,x,x,x", where x are numbers
     that represent respectively red, green, blue and alpha values (0-65535) */
  aosd_color_t color = {0, 0, 0, 65535};
  sscanf (str, "%d,%d,%d,%d", & color.red, & color.green, & color.blue, & color.alpha);
  return color;
}

static StringBuf color_to_str (const aosd_color_t & color)
{
  /* color strings are in format "x,x,x,x", where x are numbers
     that represent respectively red, green, blue and alpha values (0-65535) */
  return str_printf ("%d,%d,%d,%d", color.red, color.green, color.blue, color.alpha);
}

static const char * const aosd_defaults[] = {
 "position_placement", aud::numeric_string<AOSD_POSITION_PLACEMENT_TOPLEFT>::str,
 "position_offset_x", "0",
 "position_offset_y", "0",
 "position_maxsize_width", "0",
 "position_multimon_id", "-1",
 "animation_timing_display", "3000",
 "animation_timing_fadein", "300",
 "animation_timing_fadeout", "300",
 "text_fonts_name_0", "Sans 26",
 "text_fonts_color_0", "65535,65535,65535,65535",
 "text_fonts_draw_shadow_0", "TRUE",
 "text_fonts_shadow_color_0", "0,0,0,32767",
 "decoration_code", "0",
 "decoration_color_0", "0,0,65535,32767",
 "decoration_color_1", "65535,65535,65535,65535",
 "trigger_enabled", "1,0,0,0",
 "transparency_mode", "0",
 nullptr};

void aosd_cfg_load (aosd_cfg_t & cfg)
{
  aud_config_set_defaults ("aosd", aosd_defaults);

  /* position */
  cfg.position.placement = aud_get_int ("aosd", "position_placement");
  cfg.position.offset_x = aud_get_int ("aosd", "position_offset_x");
  cfg.position.offset_y = aud_get_int ("aosd", "position_offset_y");
  cfg.position.maxsize_width = aud_get_int ("aosd", "position_maxsize_width");
  cfg.position.multimon_id = aud_get_int ("aosd", "position_multimon_id");

  /* animation */
  cfg.animation.timing_display = aud_get_int ("aosd", "animation_timing_display");
  cfg.animation.timing_fadein = aud_get_int ("aosd", "animation_timing_fadein");
  cfg.animation.timing_fadeout = aud_get_int ("aosd", "animation_timing_fadeout");

  /* text */
  for (int i = 0; i < AOSD_TEXT_FONTS_NUM; i ++)
  {
    char key_str[32];

    snprintf (key_str, sizeof key_str, "text_fonts_name_%i" , i );
    cfg.text.fonts_name[i] = aud_get_str ("aosd", key_str);

    snprintf (key_str, sizeof key_str, "text_fonts_color_%i", i);
    cfg.text.fonts_color[i] = str_to_color (aud_get_str ("aosd", key_str));

    snprintf (key_str, sizeof key_str, "text_fonts_draw_shadow_%i", i);
    cfg.text.fonts_draw_shadow[i] = aud_get_bool ("aosd", key_str);

    snprintf (key_str, sizeof key_str, "text_fonts_shadow_color_%i", i);
    cfg.text.fonts_shadow_color[i] = str_to_color (aud_get_str ("aosd", key_str));
  }

  /* decoration */
  cfg.decoration.code = aud_get_int ("aosd", "decoration_code");

  /* decoration - colors */
  for (int i = 0; i < AOSD_DECO_STYLE_MAX_COLORS; i ++)
  {
    char key_str[32];
    snprintf (key_str, sizeof key_str, "decoration_color_%i", i);
    cfg.decoration.colors[i] = str_to_color (aud_get_str ("aosd", key_str));
  }

  /* trigger */
  String trig_enabled_str = aud_get_str ("aosd", "trigger_enabled");
  str_to_int_array (trig_enabled_str, cfg.trigger.enabled,
   aud::n_elems (cfg.trigger.enabled));

  /* miscellanous */
  cfg.misc.transparency_mode = aud_get_int ("aosd", "transparency_mode");
}


void aosd_cfg_save (const aosd_cfg_t & cfg)
{
  /* position */
  aud_set_int ("aosd", "position_placement", cfg.position.placement);
  aud_set_int ("aosd", "position_offset_x", cfg.position.offset_x);
  aud_set_int ("aosd", "position_offset_y", cfg.position.offset_y);
  aud_set_int ("aosd", "position_maxsize_width", cfg.position.maxsize_width);
  aud_set_int ("aosd", "position_multimon_id", cfg.position.multimon_id);

  /* animation */
  aud_set_int ("aosd", "animation_timing_display", cfg.animation.timing_display);
  aud_set_int ("aosd", "animation_timing_fadein", cfg.animation.timing_fadein);
  aud_set_int ("aosd", "animation_timing_fadeout", cfg.animation.timing_fadeout);

  /* text */
  for (int i = 0; i < AOSD_TEXT_FONTS_NUM; i ++)
  {
    char key_str[32];

    snprintf (key_str, sizeof key_str, "text_fonts_name_%i", i);
    aud_set_str ("aosd", key_str, cfg.text.fonts_name[i]);

    snprintf (key_str, sizeof key_str, "text_fonts_color_%i", i);
    aud_set_str ("aosd", key_str, color_to_str (cfg.text.fonts_color[i]));

    snprintf (key_str, sizeof key_str, "text_fonts_draw_shadow_%i", i);
    aud_set_bool ("aosd", key_str, cfg.text.fonts_draw_shadow[i]);

    snprintf (key_str, sizeof key_str, "text_fonts_shadow_color_%i", i);
    aud_set_str ("aosd", key_str, color_to_str (cfg.text.fonts_shadow_color[i]));
  }

  /* decoration */
  aud_set_int ("aosd" ,
    "decoration_code" , cfg.decoration.code );

  /* decoration - colors */
  for (int i = 0; i < AOSD_DECO_STYLE_MAX_COLORS; i ++)
  {
    char key_str[32];
    snprintf (key_str, sizeof key_str, "decoration_color_%i", i);
    aud_set_str ("aosd", key_str, color_to_str (cfg.decoration.colors[i]));
  }

  /* trigger */
  StringBuf trig_enabled_str = int_array_to_str (cfg.trigger.enabled,
   aud::n_elems (cfg.trigger.enabled));
  aud_set_str ("aosd", "trigger_enabled", trig_enabled_str);

  /* miscellaneous */
  aud_set_int ("aosd", "transparency_mode", cfg.misc.transparency_mode);
}
