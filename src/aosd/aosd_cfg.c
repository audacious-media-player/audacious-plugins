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
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>

static gint
aosd_cfg_util_str_to_color ( gchar * str , aosd_color_t * color )
{
  /* color strings are in format "x,x,x,x", where x are numbers
     that represent respectively red, green, blue and alpha values (0-65535) */
  gchar **str_values = g_strsplit( str , "," , 4 );
  gint col_values[4] = { 0 , 0 , 0, 65535 };
  gint i = 0;
  while ( str_values[i] != NULL )
  {
    col_values[i] = (gint)strtol( str_values[i] , NULL , 10 );
    i++;
  }
  g_strfreev( str_values );
  color->red = col_values[0];
  color->green = col_values[1];
  color->blue = col_values[2];
  color->alpha = col_values[3];
  if ( i < 4 )
    return -1;
  else
    return 0;
}


static gint
aosd_cfg_util_color_to_str ( aosd_color_t color , gchar ** str )
{
  /* color strings are in format "x,x,x,x", where x are numbers
     that represent respectively red, green, blue and alpha values (0-65535) */
  *str = g_strdup_printf( "%i,%i,%i,%i" , color.red , color.green , color.blue , color.alpha );
  if ( *str != NULL )
    return 0;
  else
    return -1;
}


aosd_cfg_t *
aosd_cfg_new ( void )
{
  aosd_cfg_t *cfg = g_malloc0(sizeof(aosd_cfg_t));
  aosd_cfg_osd_t *cfg_osd = aosd_cfg_osd_new();
  cfg->set = FALSE;
  cfg->osd = cfg_osd;
  return cfg;
}


void
aosd_cfg_delete ( aosd_cfg_t * cfg )
{
  if ( cfg != NULL )
  {
    if ( cfg->osd != NULL )
      aosd_cfg_osd_delete( cfg->osd );
    g_free( cfg );
  }
  return;
}


aosd_cfg_osd_t *
aosd_cfg_osd_new( void )
{
  aosd_cfg_osd_t *cfg_osd = g_malloc0(sizeof(aosd_cfg_osd_t));
  cfg_osd->decoration.colors = g_array_sized_new( FALSE , TRUE , sizeof(aosd_color_t) ,
                                                  aosd_deco_style_get_max_numcol() );
  cfg_osd->trigger.active = g_array_new( FALSE , TRUE , sizeof(gint) );
  return cfg_osd;
}


void
aosd_cfg_osd_delete ( aosd_cfg_osd_t * cfg_osd )
{
  if ( cfg_osd != NULL )
  {
    gint i = 0;
    /* free configuration fields */
    for ( i = 0 ; i < AOSD_TEXT_FONTS_NUM ; i++ )
      str_unref (cfg_osd->text.fonts_name[i]);
    if ( cfg_osd->decoration.colors != NULL )
      g_array_free( cfg_osd->decoration.colors , TRUE );
    if ( cfg_osd->trigger.active != NULL )
      g_array_free( cfg_osd->trigger.active , TRUE );
  }
  g_free( cfg_osd );
  return;
}


/* makes a copy of a aosd_cfg_osd_t object (mostly used by aosd_display) */
aosd_cfg_osd_t *
aosd_cfg_osd_copy ( aosd_cfg_osd_t * cfg_osd )
{
  aosd_cfg_osd_t *cfg_osd_copy = aosd_cfg_osd_new();
  gint i = 0;
  /* copy information */
  cfg_osd_copy->position.placement = cfg_osd->position.placement;
  cfg_osd_copy->position.offset_x = cfg_osd->position.offset_x;
  cfg_osd_copy->position.offset_y = cfg_osd->position.offset_y;
  cfg_osd_copy->position.maxsize_width = cfg_osd->position.maxsize_width;
  cfg_osd_copy->position.multimon_id = cfg_osd->position.multimon_id;
  cfg_osd_copy->animation.timing_display = cfg_osd->animation.timing_display;
  cfg_osd_copy->animation.timing_fadein = cfg_osd->animation.timing_fadein;
  cfg_osd_copy->animation.timing_fadeout = cfg_osd->animation.timing_fadeout;
  for ( i = 0 ; i < AOSD_TEXT_FONTS_NUM ; i++ )
  {
    cfg_osd_copy->text.fonts_name[i] = str_ref (cfg_osd->text.fonts_name[i]);
    cfg_osd_copy->text.fonts_color[i] = cfg_osd->text.fonts_color[i];
    cfg_osd_copy->text.fonts_draw_shadow[i] = cfg_osd->text.fonts_draw_shadow[i];
    cfg_osd_copy->text.fonts_shadow_color[i] = cfg_osd->text.fonts_shadow_color[i];
  }
  cfg_osd_copy->text.utf8conv_disable = cfg_osd->text.utf8conv_disable;
  cfg_osd_copy->decoration.code = cfg_osd->decoration.code;
  for ( i = 0 ; i < cfg_osd->decoration.colors->len ; i++ )
  {
    aosd_color_t color = g_array_index( cfg_osd->decoration.colors , aosd_color_t , i );
    g_array_insert_val( cfg_osd_copy->decoration.colors , i , color );
  }
  for ( i = 0 ; i < cfg_osd->trigger.active->len ; i++ )
  {
    gint trigger_id = g_array_index( cfg_osd->trigger.active , gint , i );
    g_array_insert_val( cfg_osd_copy->trigger.active , i , trigger_id );
  }
  cfg_osd_copy->misc.transparency_mode = cfg_osd->misc.transparency_mode;
  return cfg_osd_copy;
}


#ifdef DEBUG
void
aosd_cfg_debug ( aosd_cfg_t * cfg )
{
  gint i = 0;
  GString *string = g_string_new( "" );
  g_print("\n***** debug configuration *****\n\n");
  g_print("POSITION\n");
  g_print("  placement: %i\n", cfg->osd->position.placement);
  g_print("  offset x: %i\n", cfg->osd->position.offset_x);
  g_print("  offset y: %i\n", cfg->osd->position.offset_y);
  g_print("  max OSD width: %i\n", cfg->osd->position.maxsize_width);
  g_print("  multi-monitor id: %i\n", cfg->osd->position.multimon_id);
  g_print("\nANIMATION\n");
  g_print("  timing display: %i\n", cfg->osd->animation.timing_display);
  g_print("  timing fade in: %i\n", cfg->osd->animation.timing_fadein);
  g_print("  timing fade out: %i\n", cfg->osd->animation.timing_fadeout);
  g_print("\nTEXT\n");
  for ( i = 0 ; i < AOSD_TEXT_FONTS_NUM ; i++ )
  {
    g_print("  font %i: %s\n", i, cfg->osd->text.fonts_name[i]);
    g_print("  font color %i: %i,%i,%i (alpha %i)\n", i,
      cfg->osd->text.fonts_color[i].red, cfg->osd->text.fonts_color[i].green,
      cfg->osd->text.fonts_color[i].blue, cfg->osd->text.fonts_color[i].alpha);
    g_print("  font %i use shadow: %i\n", i, cfg->osd->text.fonts_draw_shadow[i]);
    g_print("  font %i shadow color: %i,%i,%i (alpha %i)\n", i,
      cfg->osd->text.fonts_shadow_color[i].red, cfg->osd->text.fonts_shadow_color[i].green,
      cfg->osd->text.fonts_shadow_color[i].blue, cfg->osd->text.fonts_shadow_color[i].alpha);
  }
  g_print("  disable utf8 conversion: %i\n", cfg->osd->text.utf8conv_disable);
  g_print("\nDECORATION\n");
  g_print("  code: %i\n", cfg->osd->decoration.code);
  for ( i = 0 ; i < cfg->osd->decoration.colors->len ; i++ )
  {
    aosd_color_t color = g_array_index( cfg->osd->decoration.colors , aosd_color_t , i );
    g_print("  color %i: %i,%i,%i (alpha %i)\n", i, color.red, color.green, color.blue, color.alpha);
  }
  g_print("\nTRIGGER\n");
  for ( i = 0 ; i < cfg->osd->trigger.active->len ; i++ )
    g_string_append_printf( string , "%i," , g_array_index( cfg->osd->trigger.active , gint , i ) );
  if ( string->len > 1 )
    g_string_truncate( string , string->len - 1 );
  g_print("  active: %s\n", string->str);
  g_string_free( string , TRUE );
  g_print("\nEXTRA\n");
  g_print("  set: %i\n", cfg->set);
  g_print("\n*******************************\n\n");
  return;
}
#endif

static const gchar * const aosd_defaults[] = {
 "position_placement", "1", /* AOSD_POSITION_PLACEMENT_TOPLEFT */
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
 "text_utf8conv_disable", "FALSE",
 "decoration_code", "0",
 "decoration_color_0", "0,0,65535,32767",
 "decoration_color_1", "65535,65535,65535,65535",
 "trigger_active", "0",
 "transparency_mode", "0",
 NULL};

gint
aosd_cfg_load ( aosd_cfg_t * cfg )
{
  aud_config_set_defaults ("aosd", aosd_defaults);

  gint i = 0;
  gint max_numcol;
  gchar *trig_active_str;

  /* position */
  cfg->osd->position.placement = aud_get_int ("aosd", "position_placement");
  cfg->osd->position.offset_x = aud_get_int ("aosd", "position_offset_x");
  cfg->osd->position.offset_y = aud_get_int ("aosd", "position_offset_y");
  cfg->osd->position.maxsize_width = aud_get_int ("aosd", "position_maxsize_width");
  cfg->osd->position.multimon_id = aud_get_int ("aosd", "position_multimon_id");

  /* animation */
  cfg->osd->animation.timing_display = aud_get_int ("aosd", "animation_timing_display");
  cfg->osd->animation.timing_fadein = aud_get_int ("aosd", "animation_timing_fadein");
  cfg->osd->animation.timing_fadeout = aud_get_int ("aosd", "animation_timing_fadeout");

  /* text */
  for ( i = 0 ; i < AOSD_TEXT_FONTS_NUM ; i++ )
  {
    gchar *color_str = NULL;
    gchar key_str[32];

    snprintf (key_str, sizeof key_str, "text_fonts_name_%i" , i );
    cfg->osd->text.fonts_name[i] = aud_get_str ("aosd", key_str);

    snprintf (key_str, sizeof key_str, "text_fonts_color_%i", i);
    color_str = aud_get_str ("aosd", key_str);
    aosd_cfg_util_str_to_color( color_str , &(cfg->osd->text.fonts_color[i]) );
    str_unref (color_str);

    snprintf (key_str, sizeof key_str, "text_fonts_draw_shadow_%i", i);
    cfg->osd->text.fonts_draw_shadow[i] = aud_get_bool ("aosd", key_str);

    snprintf (key_str, sizeof key_str, "text_fonts_shadow_color_%i", i);
    color_str = aud_get_str ("aosd", key_str);
    aosd_cfg_util_str_to_color( color_str , &(cfg->osd->text.fonts_shadow_color[i]) );
    str_unref (color_str);
  }

  cfg->osd->text.utf8conv_disable = aud_get_bool ("aosd", "text_utf8conv_disable");

  /* decoration */
  cfg->osd->decoration.code = aud_get_int ("aosd", "decoration_code");

  /* decoration - colors */
  max_numcol = aosd_deco_style_get_max_numcol();
  for ( i = 0 ; i < max_numcol ; i++ )
  {
    gchar key_str[32];
    gchar *color_str = NULL;
    aosd_color_t color;
    snprintf (key_str, sizeof key_str, "decoration_color_%i", i);
    color_str = aud_get_str ("aosd", key_str);
    aosd_cfg_util_str_to_color( color_str , &color );
    str_unref (color_str);
    g_array_insert_val( cfg->osd->decoration.colors , i , color );
  }

  /* trigger */
  trig_active_str = aud_get_str ("aosd", "trigger_active");

  if (strcmp (trig_active_str, "x"))
  {
    gchar **trig_active_strv = g_strsplit( trig_active_str , "," , 0 );
    gint j = 0;
    while ( trig_active_strv[j] != NULL )
    {
      gint trig_active_val = strtol( trig_active_strv[j] , NULL , 10 );
      g_array_append_val( cfg->osd->trigger.active , trig_active_val );
      j++;
    }
    g_strfreev( trig_active_strv );
  }

  str_unref (trig_active_str);

  /* miscellanous */
  cfg->osd->misc.transparency_mode = aud_get_int ("aosd", "transparency_mode");

  /* the config object has been filled with information */
  cfg->set = TRUE;

  return 0;
}


gint
aosd_cfg_save ( aosd_cfg_t * cfg )
{
  gint i = 0;
  gint max_numcol;
  GString *string = g_string_new( "" );

  if ( cfg->set == FALSE )
    return -1;

  /* position */
  aud_set_int ("aosd", "position_placement", cfg->osd->position.placement);
  aud_set_int ("aosd", "position_offset_x", cfg->osd->position.offset_x);
  aud_set_int ("aosd", "position_offset_y", cfg->osd->position.offset_y);
  aud_set_int ("aosd", "position_maxsize_width", cfg->osd->position.maxsize_width);
  aud_set_int ("aosd", "position_multimon_id", cfg->osd->position.multimon_id);

  /* animation */
  aud_set_int ("aosd", "animation_timing_display", cfg->osd->animation.timing_display);
  aud_set_int ("aosd", "animation_timing_fadein", cfg->osd->animation.timing_fadein);
  aud_set_int ("aosd", "animation_timing_fadeout", cfg->osd->animation.timing_fadeout);

  /* text */
  for ( i = 0 ; i < AOSD_TEXT_FONTS_NUM ; i++ )
  {
    gchar *color_str = NULL;
    gchar key_str[32];

    snprintf (key_str, sizeof key_str, "text_fonts_name_%i", i);
    aud_set_str ("aosd", key_str, cfg->osd->text.fonts_name[i]);

    snprintf (key_str, sizeof key_str, "text_fonts_color_%i", i);
    aosd_cfg_util_color_to_str( cfg->osd->text.fonts_color[i] , &color_str );
    aud_set_str ("aosd", key_str, color_str);
    g_free( color_str );

    snprintf (key_str, sizeof key_str, "text_fonts_draw_shadow_%i", i);
    aud_set_bool ("aosd", key_str, cfg->osd->text.fonts_draw_shadow[i]);

    snprintf (key_str, sizeof key_str, "text_fonts_shadow_color_%i", i);
    aosd_cfg_util_color_to_str( cfg->osd->text.fonts_shadow_color[i] , &color_str );
    aud_set_str ("aosd", key_str, color_str);
    g_free( color_str );
  }

  aud_set_bool ("aosd", "text_utf8conv_disable", cfg->osd->text.utf8conv_disable);

  /* decoration */
  aud_set_int ("aosd" ,
    "decoration_code" , cfg->osd->decoration.code );

  /* decoration - colors */
  max_numcol = aosd_deco_style_get_max_numcol();
  for ( i = 0 ; i < max_numcol ; i++ )
  {
    gchar key_str[32];
    gchar *color_str = NULL;
    aosd_color_t color = g_array_index( cfg->osd->decoration.colors , aosd_color_t , i );
    snprintf (key_str, sizeof key_str, "decoration_color_%i", i);
    aosd_cfg_util_color_to_str( color , &color_str );
    aud_set_str ("aosd", key_str, color_str);
    g_free( color_str );
  }

  /* trigger */
  for ( i = 0 ; i < cfg->osd->trigger.active->len ; i++ )
    g_string_append_printf( string , "%i," , g_array_index( cfg->osd->trigger.active , gint , i ) );
  if ( string->len > 1 )
    g_string_truncate( string , string->len - 1 );
  else
    g_string_assign( string , "x" );
  aud_set_str ("aosd", "trigger_active", string->str);
  g_string_free( string , TRUE );

  /* miscellaneous */
  aud_set_int ("aosd", "transparency_mode", cfg->osd->misc.transparency_mode);

  return 0;
}
