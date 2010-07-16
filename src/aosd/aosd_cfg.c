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
#include <stdlib.h>

#include <audacious/configdb.h>
#include <audacious/plugin.h>

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
  cfg_osd->decoration.skin_file = NULL; /* TODO paranoid, remove me when implemented */
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
    {
      if ( cfg_osd->text.fonts_name[i] != NULL )
        g_free( cfg_osd->text.fonts_name[i] );
    }
    /* TODO not implemented yet
    if ( cfg_osd->decoration.skin_file != NULL )
      g_free( cfg_osd->decoration.skin_file );
    */
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
    cfg_osd_copy->text.fonts_name[i] = g_strdup( cfg_osd->text.fonts_name[i] );
    cfg_osd_copy->text.fonts_color[i] = cfg_osd->text.fonts_color[i];
    cfg_osd_copy->text.fonts_draw_shadow[i] = cfg_osd->text.fonts_draw_shadow[i];
    cfg_osd_copy->text.fonts_shadow_color[i] = cfg_osd->text.fonts_shadow_color[i];
  }
  cfg_osd_copy->text.utf8conv_disable = cfg_osd->text.utf8conv_disable;
  cfg_osd_copy->decoration.code = cfg_osd->decoration.code;
  cfg_osd_copy->decoration.skin_file = g_strdup( cfg_osd->decoration.skin_file );
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
  /*g_print("  custom skin file: %s\n", cfg->osd->decoration.skin_file);*/
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


gint
aosd_cfg_load ( aosd_cfg_t * cfg )
{
  mcs_handle_t *cfgfile = aud_cfg_db_open();
  gint i = 0;
  gint max_numcol;
  gchar *trig_active_str;

  /* position */
  if ( !aud_cfg_db_get_int( cfgfile , "aosd" ,
       "position_placement" , &(cfg->osd->position.placement) ) )
    cfg->osd->position.placement = AOSD_POSITION_PLACEMENT_TOPLEFT;

  if ( !aud_cfg_db_get_int( cfgfile , "aosd" ,
       "position_offset_x" , &(cfg->osd->position.offset_x) ) )
    cfg->osd->position.offset_x = 0;

  if ( !aud_cfg_db_get_int( cfgfile , "aosd" ,
       "position_offset_y" , &(cfg->osd->position.offset_y) ) )
    cfg->osd->position.offset_y = 0;

  if ( !aud_cfg_db_get_int( cfgfile , "aosd" ,
       "position_maxsize_width" , &(cfg->osd->position.maxsize_width) ) )
    cfg->osd->position.maxsize_width = 0;

  if ( !aud_cfg_db_get_int( cfgfile , "aosd" ,
       "position_multimon_id" , &(cfg->osd->position.multimon_id) ) )
    cfg->osd->position.multimon_id = -1;

  /* animation */
  if ( !aud_cfg_db_get_int( cfgfile , "aosd" ,
       "animation_timing_display" , &(cfg->osd->animation.timing_display) ) )
    cfg->osd->animation.timing_display = 3000;

  if ( !aud_cfg_db_get_int( cfgfile , "aosd" ,
       "animation_timing_fadein" , &(cfg->osd->animation.timing_fadein) ) )
    cfg->osd->animation.timing_fadein = 300;

  if ( !aud_cfg_db_get_int( cfgfile , "aosd" ,
       "animation_timing_fadeout" , &(cfg->osd->animation.timing_fadeout) ) )
    cfg->osd->animation.timing_fadeout = 300;

  /* text */
  for ( i = 0 ; i < AOSD_TEXT_FONTS_NUM ; i++ )
  {
    gchar *color_str = NULL;
    gchar *key_str = NULL;
    key_str = g_strdup_printf( "text_fonts_name_%i" , i );
    if ( !aud_cfg_db_get_string( cfgfile , "aosd" , key_str , &(cfg->osd->text.fonts_name[i]) ) )
      cfg->osd->text.fonts_name[i] = g_strdup( "Sans 26" );
    g_free( key_str );
    key_str = g_strdup_printf( "text_fonts_color_%i" , i );
    if ( !aud_cfg_db_get_string( cfgfile , "aosd" , key_str , &color_str ) )
      color_str = g_strdup( "65535,65535,65535,65535" ); /* white , alpha 100% */
    aosd_cfg_util_str_to_color( color_str , &(cfg->osd->text.fonts_color[i]) );
    g_free( key_str );
    g_free( color_str );
    key_str = g_strdup_printf( "text_fonts_draw_shadow_%i" , i );
    if ( !aud_cfg_db_get_bool( cfgfile , "aosd" , key_str , &(cfg->osd->text.fonts_draw_shadow[i]) ) )
      cfg->osd->text.fonts_draw_shadow[i] = TRUE;
    g_free( key_str );
    key_str = g_strdup_printf( "text_fonts_shadow_color_%i" , i );
    if ( !aud_cfg_db_get_string( cfgfile , "aosd" , key_str , &color_str ) )
      color_str = g_strdup( "0,0,0,32767" ); /* black , alpha 50% */
    aosd_cfg_util_str_to_color( color_str , &(cfg->osd->text.fonts_shadow_color[i]) );
    g_free( key_str );
    g_free( color_str );
  }

  if ( !aud_cfg_db_get_bool( cfgfile , "aosd" ,
       "text_utf8conv_disable" , &(cfg->osd->text.utf8conv_disable) ) )
    cfg->osd->text.utf8conv_disable = FALSE;

  /* decoration */
  if ( !aud_cfg_db_get_int( cfgfile , "aosd" ,
       "decoration_code" , &(cfg->osd->decoration.code) ) )
    cfg->osd->decoration.code = aosd_deco_style_get_first_code();

  /* TODO not implemented yet
  if ( !aud_cfg_db_get_string( cfgfile , "aosd" ,
       "decoration_skin_file" , &(cfg->osd->decoration.skin_file) ) )
    cfg->osd->decoration.skin_file = g_strdup( "" );
  */

  /* decoration - colors */
  max_numcol = aosd_deco_style_get_max_numcol();
  for ( i = 0 ; i < max_numcol ; i++ )
  {
    gchar *key_str = NULL;
    gchar *color_str = NULL;
    aosd_color_t color;
    key_str = g_strdup_printf( "decoration_color_%i" , i );
    if ( !aud_cfg_db_get_string( cfgfile , "aosd" , key_str , &color_str ) )
    {
      /* we have different default values for the decoration colors */
      switch ( i )
      {
        case 0:
          color_str = g_strdup( "0,0,65535,32767" ); /* blue , alpha 50% */
          break;
        case 1:
          color_str = g_strdup( "65535,65535,65535,65535" ); /* white , alpha 100% */
          break;
        case 2:
          color_str = g_strdup( "51400,51400,51400,65535" ); /* gray , alpha 100% */
          break;
        default:
          color_str = g_strdup( "51400,51400,51400,65535" ); /* gray , alpha 100% */
          break;
      }
    }
    aosd_cfg_util_str_to_color( color_str , &color );
    g_array_insert_val( cfg->osd->decoration.colors , i , color );
  }

  /* trigger */
  if ( !aud_cfg_db_get_string( cfgfile , "aosd" , "trigger_active" , &trig_active_str ) )
  {
    gint trig_active_defval = 0;
    g_array_append_val( cfg->osd->trigger.active , trig_active_defval );
  }
  else if ( strcmp("x",trig_active_str) )
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

  /* miscellanous */
  if ( !aud_cfg_db_get_int( cfgfile , "aosd" ,
       "transparency_mode" , &(cfg->osd->misc.transparency_mode) ) )
    cfg->osd->misc.transparency_mode = AOSD_MISC_TRANSPARENCY_FAKE;

  aud_cfg_db_close( cfgfile );

  /* the config object has been filled with information */
  cfg->set = TRUE;

  return 0;
}


gint
aosd_cfg_save ( aosd_cfg_t * cfg )
{
  mcs_handle_t *cfgfile = aud_cfg_db_open();
  gint i = 0;
  gint max_numcol;
  GString *string = g_string_new( "" );

  if ( cfg->set == FALSE )
    return -1;

  /* position */
  aud_cfg_db_set_int( cfgfile , "aosd" ,
    "position_placement" , cfg->osd->position.placement );

  aud_cfg_db_set_int( cfgfile , "aosd" ,
    "position_offset_x" , cfg->osd->position.offset_x );

  aud_cfg_db_set_int( cfgfile , "aosd" ,
    "position_offset_y" , cfg->osd->position.offset_y );

  aud_cfg_db_set_int( cfgfile , "aosd" ,
    "position_maxsize_width" , cfg->osd->position.maxsize_width );

  aud_cfg_db_set_int( cfgfile , "aosd" ,
    "position_multimon_id" , cfg->osd->position.multimon_id );

  /* animation */
  aud_cfg_db_set_int( cfgfile , "aosd" ,
    "animation_timing_display" , cfg->osd->animation.timing_display );

  aud_cfg_db_set_int( cfgfile , "aosd" ,
    "animation_timing_fadein" , cfg->osd->animation.timing_fadein );

  aud_cfg_db_set_int( cfgfile , "aosd" ,
    "animation_timing_fadeout" , cfg->osd->animation.timing_fadeout );

  /* text */
  for ( i = 0 ; i < AOSD_TEXT_FONTS_NUM ; i++ )
  {
    gchar *color_str = NULL;
    gchar *key_str = NULL;
    key_str = g_strdup_printf( "text_fonts_name_%i" , i );
    aud_cfg_db_set_string( cfgfile , "aosd" ,
      key_str , cfg->osd->text.fonts_name[i] );
    g_free( key_str );
    key_str = g_strdup_printf( "text_fonts_color_%i" , i );
    aosd_cfg_util_color_to_str( cfg->osd->text.fonts_color[i] , &color_str );
    aud_cfg_db_set_string( cfgfile , "aosd" ,
      key_str , color_str );
    g_free( key_str );
    g_free( color_str );
    key_str = g_strdup_printf( "text_fonts_draw_shadow_%i" , i );
    aud_cfg_db_set_bool( cfgfile , "aosd" ,
      key_str , cfg->osd->text.fonts_draw_shadow[i] );
    g_free( key_str );
    key_str = g_strdup_printf( "text_fonts_shadow_color_%i" , i );
    aosd_cfg_util_color_to_str( cfg->osd->text.fonts_shadow_color[i] , &color_str );
    aud_cfg_db_set_string( cfgfile , "aosd" ,
      key_str , color_str );
    g_free( key_str );
    g_free( color_str );
  }

  aud_cfg_db_set_bool( cfgfile , "aosd" ,
    "text_utf8conv_disable" , cfg->osd->text.utf8conv_disable );

  /* decoration */
  aud_cfg_db_set_int( cfgfile , "aosd" ,
    "decoration_code" , cfg->osd->decoration.code );

  /* TODO skip this since it's not implemented yet
  aud_cfg_db_set_string( cfgfile , "aosd" ,
    "decoration_skin_file" , cfg->osd->decoration.skin_file ); */

  /* decoration - colors */
  max_numcol = aosd_deco_style_get_max_numcol();
  for ( i = 0 ; i < max_numcol ; i++ )
  {
    gchar *key_str = NULL;
    gchar *color_str = NULL;
    aosd_color_t color = g_array_index( cfg->osd->decoration.colors , aosd_color_t , i );
    key_str = g_strdup_printf( "decoration_color_%i" , i );
    aosd_cfg_util_color_to_str( color , &color_str );
    aud_cfg_db_set_string( cfgfile , "aosd" ,
      key_str , color_str );
    g_free( key_str );
    g_free( color_str );
  }

  /* trigger */
  for ( i = 0 ; i < cfg->osd->trigger.active->len ; i++ )
    g_string_append_printf( string , "%i," , g_array_index( cfg->osd->trigger.active , gint , i ) );
  if ( string->len > 1 )
    g_string_truncate( string , string->len - 1 );
  else
    g_string_assign( string , "x" );
  aud_cfg_db_set_string( cfgfile , "aosd" , "trigger_active" , string->str );
  g_string_free( string , TRUE );

  /* miscellaneous */
  aud_cfg_db_set_int( cfgfile , "aosd" ,
    "transparency_mode" , cfg->osd->misc.transparency_mode );

  aud_cfg_db_close( cfgfile );

  return 0;
}
