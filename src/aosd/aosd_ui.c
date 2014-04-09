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

#include <math.h>

#include <gtk/gtk.h>

#include <libaudcore/i18n.h>
#include <libaudgui/libaudgui-gtk.h>

#include "aosd_ui.h"
#include "aosd_style.h"
#include "aosd_trigger.h"
#include "aosd_cfg.h"
#include "aosd_osd.h"
#include "aosd_common.h"

extern aosd_cfg_t * global_config;
extern gboolean plugin_is_active;


static void chooser_get_aosd_color (GtkColorChooser * chooser, aosd_color_t * color)
{
  GdkRGBA rgba;
  gtk_color_chooser_get_rgba (chooser, & rgba);

  color->red = rint (rgba.red * 65535.0);
  color->green = rint (rgba.green * 65535.0);
  color->blue = rint (rgba.blue * 65535.0);
  color->alpha = rint (rgba.alpha * 65535.0);
}


static void chooser_set_aosd_color (GtkColorChooser * chooser, const aosd_color_t * color)
{
  GdkRGBA rgba =
  {
    .red = color->red / 65535.0,
    .green = color->green / 65535.0,
    .blue = color->blue / 65535.0,
    .alpha = color->alpha / 65535.0
  };

  gtk_color_chooser_set_use_alpha (chooser, TRUE);
  gtk_color_chooser_set_rgba (chooser, & rgba);
}


/*************************************************************/
/* small callback system used by the configuration interface */
typedef void (*aosd_ui_cb_func_t)( GtkWidget * , aosd_cfg_t * );

typedef struct
{
  aosd_ui_cb_func_t func;
  GtkWidget * widget;
}
aosd_ui_cb_t;

static void
aosd_callback_list_add ( GList ** list , GtkWidget * widget , aosd_ui_cb_func_t func )
{
  aosd_ui_cb_t *cb = g_malloc(sizeof(aosd_ui_cb_t));
  cb->widget = widget;
  cb->func = func;
  *list = g_list_append( *list , cb );
}

static void
aosd_callback_list_run ( GList * list , aosd_cfg_t * cfg )
{
  while ( list != NULL )
  {
    aosd_ui_cb_t *cb = (aosd_ui_cb_t*)list->data;
    cb->func( cb->widget , cfg );
    list = g_list_next( list );
  }
}

static void
aosd_callback_list_free ( GList * list )
{
  GList *list_top = list;
  while ( list != NULL )
  {
    g_free( (aosd_ui_cb_t*)list->data );
    list = g_list_next( list );
  }
  g_list_free( list_top );
}
/*************************************************************/



static gboolean
aosd_cb_configure_position_expose ( GtkWidget * darea ,
                                    cairo_t * cr ,
                                    gpointer coord_gp )
{
  gint coord = GPOINTER_TO_INT(coord_gp);

  cairo_set_source_rgb ( cr , 0 , 0 , 0 );
  cairo_rectangle ( cr , (coord % 3) * 10 , (coord / 3) * 16 , 20 , 8 );
  cairo_fill ( cr );

  return FALSE;
}


static void
aosd_cb_configure_position_placement_commit ( GtkWidget * grid , aosd_cfg_t * cfg )
{
  GList *placbt_list = gtk_container_get_children( GTK_CONTAINER(grid) );
  GList *list_iter = placbt_list;

  while ( list_iter != NULL )
  {
    GtkWidget *placbt = list_iter->data;
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(placbt) ) == TRUE )
    {
      cfg->osd->position.placement = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(placbt),"value"));
      break;
    }
    list_iter = g_list_next( list_iter );
  }

  g_list_free( placbt_list );
}


static void
aosd_cb_configure_position_offset_commit ( GtkWidget * grid , aosd_cfg_t * cfg )
{
  cfg->osd->position.offset_x = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(grid),"offx")) );
  cfg->osd->position.offset_y = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(grid),"offy")) );
}


static void
aosd_cb_configure_position_maxsize_commit ( GtkWidget * grid , aosd_cfg_t * cfg )
{
  cfg->osd->position.maxsize_width = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(grid),"maxsize_width")) );
}


static void
aosd_cb_configure_position_multimon_commit ( GtkWidget * combo , aosd_cfg_t * cfg )
{
  gint active = gtk_combo_box_get_active( GTK_COMBO_BOX(combo) );
  cfg->osd->position.multimon_id = ( active > -1 ) ? (active - 1) : -1;
}


static GtkWidget *
aosd_ui_configure_position ( aosd_cfg_t * cfg , GList ** cb_list )
{
  GtkWidget *pos_vbox;
  GtkWidget *pos_placement_frame, *pos_placement_hbox, *pos_placement_grid;
  GtkWidget *pos_placement_bt[9], *pos_placement_bt_darea[9];
  GtkWidget *pos_offset_grid, *pos_offset_x_label, *pos_offset_x_spinbt;
  GtkWidget *pos_offset_y_label, *pos_offset_y_spinbt;
  GtkWidget *pos_maxsize_width_label, *pos_maxsize_width_spinbt;
  GtkWidget *pos_multimon_frame, *pos_multimon_hbox;
  GtkWidget *pos_multimon_label;
  GtkWidget *pos_multimon_combobox;
  gint monitors_num = gdk_screen_get_n_monitors( gdk_screen_get_default() );
  gint i = 0;

  pos_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(pos_vbox) , 6 );

  pos_placement_frame = gtk_frame_new( _("Placement") );
  pos_placement_hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 0 );
  gtk_container_set_border_width( GTK_CONTAINER(pos_placement_hbox) , 6 );
  gtk_container_add( GTK_CONTAINER(pos_placement_frame) , pos_placement_hbox );
  gtk_box_pack_start( GTK_BOX(pos_vbox) , pos_placement_frame , FALSE , FALSE , 0 );

  pos_placement_grid = gtk_grid_new();
  for ( i = 0 ; i < 9 ; i++ )
  {
    if ( i == 0 )
      pos_placement_bt[i] = gtk_radio_button_new( NULL );
    else
      pos_placement_bt[i] = gtk_radio_button_new_from_widget( GTK_RADIO_BUTTON(pos_placement_bt[0]) );
    gtk_toggle_button_set_mode( GTK_TOGGLE_BUTTON(pos_placement_bt[i]) , FALSE );
    pos_placement_bt_darea[i] = gtk_drawing_area_new();
    gtk_widget_set_size_request( pos_placement_bt_darea[i] , 40 , 40 );
    gtk_container_add( GTK_CONTAINER(pos_placement_bt[i]) , pos_placement_bt_darea[i] );
    g_signal_connect( G_OBJECT(pos_placement_bt_darea[i]) , "draw" ,
                      G_CALLBACK(aosd_cb_configure_position_expose) , GINT_TO_POINTER(i) );
    gtk_grid_attach( GTK_GRID(pos_placement_grid) , pos_placement_bt[i] , (i % 3) , (i / 3) , 1 , 1 );
    g_object_set_data( G_OBJECT(pos_placement_bt[i]) , "value" , GINT_TO_POINTER(i+1) );
    if ( cfg->osd->position.placement == (i+1) )
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(pos_placement_bt[i]) , TRUE );
  }
  gtk_box_pack_start( GTK_BOX(pos_placement_hbox) , pos_placement_grid , FALSE , FALSE , 0 );
  aosd_callback_list_add( cb_list , pos_placement_grid , aosd_cb_configure_position_placement_commit );

  gtk_box_pack_start( GTK_BOX(pos_placement_hbox) , gtk_separator_new(GTK_ORIENTATION_VERTICAL) , FALSE , FALSE , 6 );

  pos_offset_grid = gtk_grid_new();
  gtk_grid_set_row_spacing( GTK_GRID(pos_offset_grid) , 4 );
  gtk_grid_set_column_spacing( GTK_GRID(pos_offset_grid) , 4 );
  pos_offset_x_label = gtk_label_new( _( "Relative X offset:" ) );
  gtk_misc_set_alignment( GTK_MISC(pos_offset_x_label) , 0 , 0.5 );
  gtk_grid_attach( GTK_GRID(pos_offset_grid) , pos_offset_x_label , 0 , 0 , 1 , 1 );
  pos_offset_x_spinbt = gtk_spin_button_new_with_range( -9999 , 9999 , 1 );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(pos_offset_x_spinbt) , cfg->osd->position.offset_x );
  gtk_grid_attach( GTK_GRID(pos_offset_grid) , pos_offset_x_spinbt , 1 , 0 , 1 , 1 );
  g_object_set_data( G_OBJECT(pos_offset_grid) , "offx" , pos_offset_x_spinbt );
  pos_offset_y_label = gtk_label_new( _( "Relative Y offset:" ) );
  gtk_misc_set_alignment( GTK_MISC(pos_offset_y_label) , 0 , 0.5 );
  gtk_grid_attach( GTK_GRID(pos_offset_grid) , pos_offset_y_label , 0 , 1 , 1 , 1 );
  pos_offset_y_spinbt = gtk_spin_button_new_with_range( -9999 , 9999 , 1 );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(pos_offset_y_spinbt) , cfg->osd->position.offset_y );
  gtk_grid_attach( GTK_GRID(pos_offset_grid) , pos_offset_y_spinbt , 1 , 1 , 1 , 1 );
  g_object_set_data( G_OBJECT(pos_offset_grid) , "offy" , pos_offset_y_spinbt );
  pos_maxsize_width_label = gtk_label_new( _("Max OSD width:") );
  gtk_misc_set_alignment( GTK_MISC(pos_maxsize_width_label) , 0 , 0.5 );
  gtk_grid_attach( GTK_GRID(pos_offset_grid) , pos_maxsize_width_label , 0 , 2 , 1 , 1 );
  pos_maxsize_width_spinbt = gtk_spin_button_new_with_range( 0 , 99999 , 1 );
  g_object_set_data( G_OBJECT(pos_offset_grid) , "maxsize_width" , pos_maxsize_width_spinbt );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(pos_maxsize_width_spinbt) , cfg->osd->position.maxsize_width );
  gtk_grid_attach( GTK_GRID(pos_offset_grid) , pos_maxsize_width_spinbt , 1 , 2 , 1 , 1 );
  gtk_box_pack_start( GTK_BOX(pos_placement_hbox) , pos_offset_grid , FALSE , FALSE , 0 );
  aosd_callback_list_add( cb_list , pos_offset_grid , aosd_cb_configure_position_offset_commit );
  aosd_callback_list_add( cb_list , pos_offset_grid , aosd_cb_configure_position_maxsize_commit );

  pos_multimon_frame = gtk_frame_new( _("Multi-Monitor options") );
  pos_multimon_hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(pos_multimon_hbox) , 6 );
  gtk_container_add( GTK_CONTAINER(pos_multimon_frame), pos_multimon_hbox );
  pos_multimon_label = gtk_label_new( _("Display OSD using:") );
  pos_multimon_combobox = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text ((GtkComboBoxText *) pos_multimon_combobox, _("all monitors"));
  for ( i = 0 ; i < monitors_num ; i++ )
  {
    gchar *mon_str = g_strdup_printf( _("monitor %i") , i + 1 );
    gtk_combo_box_text_append_text ((GtkComboBoxText *) pos_multimon_combobox, mon_str);
    g_free( mon_str );
  }
  gtk_combo_box_set_active( GTK_COMBO_BOX(pos_multimon_combobox) , (cfg->osd->position.multimon_id + 1) );
  aosd_callback_list_add( cb_list , pos_multimon_combobox , aosd_cb_configure_position_multimon_commit );
  gtk_box_pack_start( GTK_BOX(pos_multimon_hbox) , pos_multimon_label , FALSE , FALSE , 0 );
  gtk_box_pack_start( GTK_BOX(pos_multimon_hbox) , pos_multimon_combobox , FALSE , FALSE , 0 );
  gtk_box_pack_start( GTK_BOX(pos_vbox) , pos_multimon_frame , FALSE , FALSE , 0 );

  return pos_vbox;
}


static GtkWidget *
aosd_ui_configure_animation_timing ( gchar * label_string )
{
  GtkWidget *hbox, *desc_label, *spinbt;
  hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 4 );
  desc_label = gtk_label_new( label_string );
  spinbt = gtk_spin_button_new_with_range( 0 , 99999 , 1 );
  gtk_box_pack_start( GTK_BOX(hbox) , desc_label , FALSE , FALSE , 0 );
  gtk_box_pack_start( GTK_BOX(hbox) , spinbt , FALSE , FALSE , 0 );
  g_object_set_data( G_OBJECT(hbox) , "spinbt" , spinbt );
  return hbox;
}


static void
aosd_cb_configure_animation_timing_commit ( GtkWidget * timing_hbox , aosd_cfg_t * cfg )
{
  cfg->osd->animation.timing_display = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(timing_hbox),"display")) );
  cfg->osd->animation.timing_fadein = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(timing_hbox),"fadein")) );
  cfg->osd->animation.timing_fadeout = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(timing_hbox),"fadeout")) );
}


static GtkWidget *
aosd_ui_configure_animation ( aosd_cfg_t * cfg , GList ** cb_list )
{
  GtkWidget *ani_vbox;
  GtkWidget *ani_timing_frame, *ani_timing_hbox;
  GtkWidget *ani_timing_fadein_widget, *ani_timing_fadeout_widget, *ani_timing_stay_widget;
  GtkSizeGroup *sizegroup;

  ani_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , 0 );
  gtk_container_set_border_width( GTK_CONTAINER(ani_vbox) , 6 );

  ani_timing_hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 0 );
  ani_timing_frame = gtk_frame_new( _("Timing (ms)") );
  gtk_container_set_border_width( GTK_CONTAINER(ani_timing_hbox) , 6 );
  gtk_container_add( GTK_CONTAINER(ani_timing_frame) , ani_timing_hbox );
  gtk_box_pack_start( GTK_BOX(ani_vbox) , ani_timing_frame , FALSE , FALSE , 0 );

  ani_timing_stay_widget = aosd_ui_configure_animation_timing( _("Display:") );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(g_object_get_data(
    G_OBJECT(ani_timing_stay_widget),"spinbt")) , cfg->osd->animation.timing_display );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , ani_timing_stay_widget , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , gtk_separator_new(GTK_ORIENTATION_VERTICAL) , FALSE , FALSE , 4 );
  ani_timing_fadein_widget = aosd_ui_configure_animation_timing( _("Fade in:") );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(g_object_get_data(
    G_OBJECT(ani_timing_fadein_widget),"spinbt")) , cfg->osd->animation.timing_fadein );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , ani_timing_fadein_widget , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , gtk_separator_new(GTK_ORIENTATION_VERTICAL) , FALSE , FALSE , 4 );
  ani_timing_fadeout_widget = aosd_ui_configure_animation_timing( _("Fade out:") );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(g_object_get_data(
    G_OBJECT(ani_timing_fadeout_widget),"spinbt")) , cfg->osd->animation.timing_fadeout );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , ani_timing_fadeout_widget , TRUE , TRUE , 0 );
  g_object_set_data( G_OBJECT(ani_timing_hbox) , "display" ,
    g_object_get_data(G_OBJECT(ani_timing_stay_widget),"spinbt") );
  g_object_set_data( G_OBJECT(ani_timing_hbox) , "fadein" ,
    g_object_get_data(G_OBJECT(ani_timing_fadein_widget),"spinbt") );
  g_object_set_data( G_OBJECT(ani_timing_hbox) , "fadeout" ,
    g_object_get_data(G_OBJECT(ani_timing_fadeout_widget),"spinbt") );
  sizegroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
  gtk_size_group_add_widget( sizegroup , ani_timing_stay_widget );
  gtk_size_group_add_widget( sizegroup , ani_timing_fadein_widget );
  gtk_size_group_add_widget( sizegroup , ani_timing_fadeout_widget );
  aosd_callback_list_add( cb_list , ani_timing_hbox , aosd_cb_configure_animation_timing_commit );

  return ani_vbox;
}


static void
aosd_cb_configure_text_font_shadow_toggle ( GtkToggleButton * shadow_togglebt ,
                                            gpointer shadow_colorbt )
{
  if ( gtk_toggle_button_get_active( shadow_togglebt ) == TRUE )
    gtk_widget_set_sensitive( GTK_WIDGET(shadow_colorbt) , TRUE );
  else
    gtk_widget_set_sensitive( GTK_WIDGET(shadow_colorbt) , FALSE );
}


static void
aosd_cb_configure_text_font_commit ( GtkWidget * fontbt , aosd_cfg_t * cfg )
{
  gint fontnum = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(fontbt) , "fontnum" ));
  GtkColorChooser * chooser;

  str_unref (cfg->osd->text.fonts_name[fontnum]);
  cfg->osd->text.fonts_name[fontnum] =
   str_get (gtk_font_button_get_font_name (GTK_FONT_BUTTON (fontbt)));

  cfg->osd->text.fonts_draw_shadow[fontnum] = gtk_toggle_button_get_active(
    GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(fontbt),"use_shadow")) );

  chooser = g_object_get_data ((GObject *) fontbt, "color");
  chooser_get_aosd_color (chooser, & cfg->osd->text.fonts_color[fontnum]);

  chooser = g_object_get_data ((GObject *) fontbt, "shadow_color");
  chooser_get_aosd_color (chooser, & cfg->osd->text.fonts_shadow_color[fontnum]);
}


static GtkWidget *
aosd_ui_configure_text ( aosd_cfg_t * cfg , GList ** cb_list )
{
  GtkWidget *tex_vbox;
  GtkWidget *tex_font_grid, *tex_font_frame;
  GtkWidget *tex_font_label[3], *tex_font_fontbt[3];
  GtkWidget *tex_font_colorbt[3], *tex_font_shadow_togglebt[3];
  GtkWidget *tex_font_shadow_colorbt[3];
  gint i = 0;

  tex_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(tex_vbox) , 6 );

  tex_font_frame = gtk_frame_new( _("Fonts") );
  tex_font_grid = gtk_grid_new();
  gtk_container_set_border_width( GTK_CONTAINER(tex_font_grid) , 6 );
  gtk_grid_set_row_spacing ( GTK_GRID(tex_font_grid) , 4 );
  gtk_grid_set_column_spacing ( GTK_GRID(tex_font_grid) , 4 );
  for ( i = 0 ; i < AOSD_TEXT_FONTS_NUM ; i++ )
  {
    gchar *label_str = g_strdup_printf( _("Font %i:") , i+1 );
    tex_font_label[i] = gtk_label_new( label_str );
    g_free( label_str );
    tex_font_fontbt[i] = gtk_font_button_new();
    gtk_font_button_set_show_style( GTK_FONT_BUTTON(tex_font_fontbt[i]) , TRUE );
    gtk_font_button_set_show_size( GTK_FONT_BUTTON(tex_font_fontbt[i]) , TRUE );
    gtk_font_button_set_use_font( GTK_FONT_BUTTON(tex_font_fontbt[i]) , FALSE );
    gtk_font_button_set_use_size( GTK_FONT_BUTTON(tex_font_fontbt[i]) , FALSE );
    gtk_font_button_set_font_name( GTK_FONT_BUTTON(tex_font_fontbt[i]) , cfg->osd->text.fonts_name[i] );
    gtk_widget_set_hexpand( tex_font_fontbt[i] , TRUE );

    tex_font_colorbt[i] = gtk_color_button_new ();
    chooser_set_aosd_color ((GtkColorChooser *) tex_font_colorbt[i],
     & cfg->osd->text.fonts_color[i]);

    tex_font_shadow_togglebt[i] = gtk_toggle_button_new_with_label( _("Shadow") );
    gtk_toggle_button_set_mode( GTK_TOGGLE_BUTTON(tex_font_shadow_togglebt[i]) , FALSE );

    tex_font_shadow_colorbt[i] = gtk_color_button_new ();
    chooser_set_aosd_color ((GtkColorChooser *) tex_font_shadow_colorbt[i],
     & cfg->osd->text.fonts_shadow_color[i]);

    gtk_widget_set_sensitive( tex_font_shadow_colorbt[i] , FALSE );
    g_signal_connect( G_OBJECT(tex_font_shadow_togglebt[i]) , "toggled" ,
                      G_CALLBACK(aosd_cb_configure_text_font_shadow_toggle) ,
                      tex_font_shadow_colorbt[i] );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(tex_font_shadow_togglebt[i]) ,
      cfg->osd->text.fonts_draw_shadow[i] );
    gtk_grid_attach( GTK_GRID(tex_font_grid) , tex_font_label[i] , 0 , 0 , 1 , 1 );
    gtk_grid_attach( GTK_GRID(tex_font_grid) , tex_font_fontbt[i] , 1 , 0 , 1 , 1 );
    gtk_grid_attach( GTK_GRID(tex_font_grid) , tex_font_colorbt[i] , 2 , 0 , 1 , 1 );
    gtk_grid_attach( GTK_GRID(tex_font_grid) , tex_font_shadow_togglebt[i] , 3 , 0 , 1 , 1 );
    gtk_grid_attach( GTK_GRID(tex_font_grid) , tex_font_shadow_colorbt[i] , 4 , 0 , 1 , 1 );
    g_object_set_data( G_OBJECT(tex_font_fontbt[i]) , "fontnum" , GINT_TO_POINTER(i) );
    g_object_set_data( G_OBJECT(tex_font_fontbt[i]) , "color" , tex_font_colorbt[i] );
    g_object_set_data( G_OBJECT(tex_font_fontbt[i]) , "use_shadow" , tex_font_shadow_togglebt[i] );
    g_object_set_data( G_OBJECT(tex_font_fontbt[i]) , "shadow_color" , tex_font_shadow_colorbt[i] );
    aosd_callback_list_add( cb_list , tex_font_fontbt[i] , aosd_cb_configure_text_font_commit );
  }
  gtk_container_add( GTK_CONTAINER(tex_font_frame) , tex_font_grid );
  gtk_box_pack_start( GTK_BOX(tex_vbox) , tex_font_frame , FALSE , FALSE , 0 );

  return tex_vbox;
}


static void
aosd_cb_configure_decoration_style_commit ( GtkWidget * lv , aosd_cfg_t * cfg )
{
  GtkTreeSelection *sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(lv) );
  GtkTreeModel *model;
  GtkTreeIter iter;

  if ( gtk_tree_selection_get_selected( sel , &model , &iter ) == TRUE )
  {
    gint deco_code = 0;
    gtk_tree_model_get( model , &iter , 1 , &deco_code , -1 );
    cfg->osd->decoration.code = deco_code;
  }
}


static void
aosd_cb_configure_decoration_color_commit ( GtkWidget * colorbt , aosd_cfg_t * cfg )
{
  aosd_color_t color;
  chooser_get_aosd_color ((GtkColorChooser *) colorbt, & color);

  gint colnum = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(colorbt) , "colnum" ) );
  g_array_insert_val( cfg->osd->decoration.colors , colnum , color );
}


static GtkWidget *
aosd_ui_configure_decoration ( aosd_cfg_t * cfg , GList ** cb_list )
{
  GtkWidget *dec_hbox;
  GtkWidget *dec_rstyle_lv, *dec_rstyle_lv_frame, *dec_rstyle_lv_sw;
  GtkListStore *dec_rstyle_store;
  GtkCellRenderer *dec_rstyle_lv_rndr_text;
  GtkTreeViewColumn *dec_rstyle_lv_col_desc;
  GtkTreeSelection *dec_rstyle_lv_sel;
  GtkTreeIter iter, iter_sel;
  GtkWidget *dec_rstyle_hbox;
  GtkWidget *dec_rstyleopts_frame, *dec_rstyleopts_grid;
  gint *deco_code_array, deco_code_array_size;
  gint colors_max_num = 0, i = 0;

  dec_hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(dec_hbox) , 6 );

  /* decoration style model
     ---------------------------------------------
     G_TYPE_STRING -> decoration description
     G_TYPE_INT -> decoration code
     G_TYPE_INT -> number of user-definable colors
     ---------------------------------------------
  */
  dec_rstyle_store = gtk_list_store_new( 3 , G_TYPE_STRING , G_TYPE_INT , G_TYPE_INT );
  aosd_deco_style_get_codes_array ( &deco_code_array , &deco_code_array_size );
  for ( i = 0 ; i < deco_code_array_size ; i++ )
  {
    gint colors_num = aosd_deco_style_get_numcol( deco_code_array[i] );
    if ( colors_num > colors_max_num )
      colors_max_num = colors_num;
    gtk_list_store_append( dec_rstyle_store , &iter );
    gtk_list_store_set( dec_rstyle_store , &iter ,
      0 , _(aosd_deco_style_get_desc( deco_code_array[i] )) ,
      1 , deco_code_array[i] , 2 , colors_num , -1 );
    if ( deco_code_array[i] == cfg->osd->decoration.code )
      iter_sel = iter;
  }

  dec_rstyle_lv_frame = gtk_frame_new( NULL );
  dec_rstyle_lv = gtk_tree_view_new_with_model( GTK_TREE_MODEL(dec_rstyle_store) );
  g_object_unref( dec_rstyle_store );
  dec_rstyle_lv_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(dec_rstyle_lv) );
  gtk_tree_selection_set_mode( dec_rstyle_lv_sel , GTK_SELECTION_BROWSE );

  dec_rstyle_lv_rndr_text = gtk_cell_renderer_text_new();
  dec_rstyle_lv_col_desc = gtk_tree_view_column_new_with_attributes(
    _("Render Style") , dec_rstyle_lv_rndr_text , "text" , 0 , NULL );
  gtk_tree_view_append_column( GTK_TREE_VIEW(dec_rstyle_lv), dec_rstyle_lv_col_desc );
  dec_rstyle_lv_sw = gtk_scrolled_window_new( NULL , NULL );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(dec_rstyle_lv_sw) ,
                                  GTK_POLICY_NEVER , GTK_POLICY_ALWAYS );
  gtk_container_add( GTK_CONTAINER(dec_rstyle_lv_sw) , dec_rstyle_lv );
  gtk_container_add( GTK_CONTAINER(dec_rstyle_lv_frame) , dec_rstyle_lv_sw );

  gtk_tree_selection_select_iter( dec_rstyle_lv_sel , &iter_sel );
  gtk_box_pack_start( GTK_BOX(dec_hbox) , dec_rstyle_lv_frame , FALSE , FALSE , 0 );
  aosd_callback_list_add( cb_list , dec_rstyle_lv , aosd_cb_configure_decoration_style_commit );

  dec_rstyle_hbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , 4 );
  gtk_box_pack_start( GTK_BOX(dec_hbox) , dec_rstyle_hbox , TRUE , TRUE , 0 );

  /* in colors_max_num now there's the maximum number of colors used by decoration styles */
  dec_rstyleopts_frame = gtk_frame_new( _("Colors") );
  dec_rstyleopts_grid = gtk_grid_new();
  gtk_container_set_border_width( GTK_CONTAINER(dec_rstyleopts_grid) , 6 );
  gtk_grid_set_row_spacing( GTK_GRID(dec_rstyleopts_grid) , 4 );
  gtk_grid_set_column_spacing( GTK_GRID(dec_rstyleopts_grid) , 8 );
  gtk_container_add( GTK_CONTAINER(dec_rstyleopts_frame) , dec_rstyleopts_grid );
  for ( i = 0 ; i < colors_max_num ; i++ )
  {
    GtkWidget *hbox, *label;
    gchar *label_str = NULL;
    hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 4 );
    label_str = g_strdup_printf( _("Color %i:") , i+1 );
    label = gtk_label_new( label_str );
    g_free( label_str );

    GtkWidget * colorbt = gtk_color_button_new ();
    chooser_set_aosd_color ((GtkColorChooser *) colorbt,
     & g_array_index (cfg->osd->decoration.colors, aosd_color_t, i));

    gtk_box_pack_start( GTK_BOX(hbox) , label , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(hbox) , colorbt , FALSE , FALSE , 0 );
    gtk_grid_attach( GTK_GRID(dec_rstyleopts_grid) , hbox , (i % 3) , (i / 3) , 1 , 1 );
    g_object_set_data( G_OBJECT(colorbt) , "colnum" , GINT_TO_POINTER(i) );
    aosd_callback_list_add( cb_list , colorbt , aosd_cb_configure_decoration_color_commit );
  }
  gtk_box_pack_start( GTK_BOX(dec_rstyle_hbox) , dec_rstyleopts_frame , FALSE , FALSE , 0 );

  return dec_hbox;
}


static void
aosd_cb_configure_trigger_lvchanged ( GtkTreeSelection *sel , gpointer nb )
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  if ( gtk_tree_selection_get_selected( sel , &model , &iter ) == TRUE )
  {
    gint page_num = 0;
    gtk_tree_model_get( model , &iter , 2 , &page_num , -1 );
    gtk_notebook_set_current_page( GTK_NOTEBOOK(nb) , page_num );
  }
}


static gboolean
aosd_cb_configure_trigger_findinarr ( GArray * array , gint value )
{
  gint i = 0;
  for ( i = 0 ; i < array->len ; i++ )
  {
    if ( g_array_index( array , gint , i ) == value )
      return TRUE;
  }
  return FALSE;
}


static void
aosd_cb_configure_trigger_commit ( GtkWidget * cbt , aosd_cfg_t * cfg )
{
  if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(cbt) ) == TRUE )
  {
    gint value = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(cbt),"code"));
    g_array_append_val( cfg->osd->trigger.active , value );
  }
}


static GtkWidget *
aosd_ui_configure_trigger ( aosd_cfg_t * cfg , GList ** cb_list )
{
  GtkWidget *tri_hbox;
  GtkWidget *tri_event_lv, *tri_event_lv_frame, *tri_event_lv_sw;
  GtkListStore *tri_event_store;
  GtkCellRenderer *tri_event_lv_rndr_text;
  GtkTreeViewColumn *tri_event_lv_col_desc;
  GtkTreeSelection *tri_event_lv_sel;
  GtkTreeIter iter;
  gint *trigger_code_array, trigger_code_array_size;
  GtkWidget *tri_event_nb;
  gint i = 0;

  tri_event_nb = gtk_notebook_new();
  gtk_notebook_set_tab_pos( GTK_NOTEBOOK(tri_event_nb) , GTK_POS_LEFT );
  gtk_notebook_set_show_tabs( GTK_NOTEBOOK(tri_event_nb) , FALSE );
  gtk_notebook_set_show_border( GTK_NOTEBOOK(tri_event_nb) , FALSE );

  tri_hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(tri_hbox) , 6 );

  /* trigger model
     ---------------------------------------------
     G_TYPE_STRING -> trigger description
     G_TYPE_INT -> trigger code
     G_TYPE_INT -> gtk notebook page number
     ---------------------------------------------
  */
  tri_event_store = gtk_list_store_new( 3 , G_TYPE_STRING , G_TYPE_INT , G_TYPE_INT );
  aosd_trigger_get_codes_array ( &trigger_code_array , &trigger_code_array_size );
  for ( i = 0 ; i < trigger_code_array_size ; i ++ )
  {
    GtkWidget *frame, *vbox, *label, *checkbt;
    gtk_list_store_append( tri_event_store , &iter );
    gtk_list_store_set( tri_event_store , &iter ,
      0 , _(aosd_trigger_get_name( trigger_code_array[i] )) ,
      1 , trigger_code_array[i] , 2 , i , -1 );
    vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , 0 );
    gtk_container_set_border_width( GTK_CONTAINER(vbox) , 6 );
    label = gtk_label_new( _(aosd_trigger_get_desc( trigger_code_array[i] )) );
    gtk_label_set_line_wrap( GTK_LABEL(label) , TRUE );
    gtk_label_set_max_width_chars( GTK_LABEL(label), 40 );
    gtk_misc_set_alignment( GTK_MISC(label) , 0.0 , 0.0 );
    checkbt = gtk_check_button_new_with_label( _("Enable trigger") );
    if ( aosd_cb_configure_trigger_findinarr( cfg->osd->trigger.active , trigger_code_array[i] ) )
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(checkbt) , TRUE );
    else
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(checkbt) , FALSE );
    gtk_box_pack_start( GTK_BOX(vbox) , checkbt , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(vbox) , gtk_separator_new(GTK_ORIENTATION_HORIZONTAL) , FALSE , FALSE , 4 );
    gtk_box_pack_start( GTK_BOX(vbox) , label , FALSE , FALSE , 0 );
    frame = gtk_frame_new( NULL );
    gtk_container_add( GTK_CONTAINER(frame) , vbox );
    gtk_notebook_append_page( GTK_NOTEBOOK(tri_event_nb) , frame , NULL );
    g_object_set_data( G_OBJECT(checkbt) , "code" , GINT_TO_POINTER(trigger_code_array[i]) );
    aosd_callback_list_add( cb_list , checkbt , aosd_cb_configure_trigger_commit );
  }

  tri_event_lv_frame = gtk_frame_new( NULL );
  tri_event_lv = gtk_tree_view_new_with_model( GTK_TREE_MODEL(tri_event_store) );
  g_object_unref( tri_event_store );
  tri_event_lv_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(tri_event_lv) );
  gtk_tree_selection_set_mode( tri_event_lv_sel , GTK_SELECTION_BROWSE );
  g_signal_connect( G_OBJECT(tri_event_lv_sel) , "changed" ,
                    G_CALLBACK(aosd_cb_configure_trigger_lvchanged) , tri_event_nb );
  if ( gtk_tree_model_get_iter_first( GTK_TREE_MODEL(tri_event_store) , &iter ) == TRUE )
    gtk_tree_selection_select_iter( tri_event_lv_sel , &iter );

  tri_event_lv_rndr_text = gtk_cell_renderer_text_new();
  tri_event_lv_col_desc = gtk_tree_view_column_new_with_attributes(
    _("Event") , tri_event_lv_rndr_text , "text" , 0 , NULL );
  gtk_tree_view_append_column( GTK_TREE_VIEW(tri_event_lv), tri_event_lv_col_desc );
  tri_event_lv_sw = gtk_scrolled_window_new( NULL , NULL );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(tri_event_lv_sw) ,
                                  GTK_POLICY_NEVER , GTK_POLICY_ALWAYS );
  gtk_container_add( GTK_CONTAINER(tri_event_lv_sw) , tri_event_lv );
  gtk_container_add( GTK_CONTAINER(tri_event_lv_frame) , tri_event_lv_sw );
  gtk_tree_selection_select_iter( tri_event_lv_sel , &iter );

  gtk_box_pack_start( GTK_BOX(tri_hbox) , tri_event_lv_frame , FALSE , FALSE , 0 );

  gtk_box_pack_start( GTK_BOX(tri_hbox) , tri_event_nb , TRUE , TRUE , 0 );

  return tri_hbox;
}


#ifdef HAVE_XCOMPOSITE
static void
aosd_cb_configure_misc_transp_real_clicked ( GtkToggleButton * real_rbt , gpointer status_hbox )
{
  GtkWidget *img = g_object_get_data( G_OBJECT(status_hbox) , "img" );
  GtkWidget *label = g_object_get_data( G_OBJECT(status_hbox) , "label" );
  if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(real_rbt) ) )
  {
    if ( aosd_osd_check_composite_mgr() )
    {
      gtk_image_set_from_icon_name( GTK_IMAGE(img) , "face-smile" , GTK_ICON_SIZE_MENU );
      gtk_label_set_text( GTK_LABEL(label) , _("Composite manager detected") );
      gtk_widget_set_sensitive( GTK_WIDGET(status_hbox) , TRUE );
    }
    else
    {
      gtk_image_set_from_icon_name( GTK_IMAGE(img) , "dialog-warning" , GTK_ICON_SIZE_MENU );
      gtk_label_set_text( GTK_LABEL(label) ,
        _("Composite manager not detected;\nunless you know that you have one running, "
          "please activate a composite manager otherwise the OSD won't work properly") );
      gtk_widget_set_sensitive( GTK_WIDGET(status_hbox) , TRUE );
    }
  }
  else
  {
    gtk_image_set_from_icon_name( GTK_IMAGE(img) , "dialog-information" , GTK_ICON_SIZE_MENU );
    gtk_label_set_text( GTK_LABEL(label) , _("Composite manager not required for fake transparency") );
    gtk_widget_set_sensitive( GTK_WIDGET(status_hbox) , FALSE );
  }
}
#endif


static void
aosd_cb_configure_misc_transp_commit ( GtkWidget * mis_transp_vbox , aosd_cfg_t * cfg )
{
  GList *child_list = gtk_container_get_children( GTK_CONTAINER(mis_transp_vbox) );
  while (child_list != NULL)
  {
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(child_list->data) ) )
    {
      cfg->osd->misc.transparency_mode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child_list->data),"val"));
      break;
    }
    child_list = g_list_next(child_list);
  }
}


static GtkWidget *
aosd_ui_configure_misc ( aosd_cfg_t * cfg , GList ** cb_list )
{
  GtkWidget *mis_vbox;
  GtkWidget *mis_transp_frame, *mis_transp_vbox;
  GtkWidget *mis_transp_fake_rbt, *mis_transp_real_rbt;
  GtkWidget *mis_transp_status_frame, *mis_transp_status_hbox;
  GtkWidget *mis_transp_status_img, *mis_transp_status_label;

  mis_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , 0 );
  gtk_container_set_border_width( GTK_CONTAINER(mis_vbox) , 6 );

  mis_transp_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , 0 );
  mis_transp_frame = gtk_frame_new( _("Transparency") );
  gtk_container_set_border_width( GTK_CONTAINER(mis_transp_vbox) , 6 );
  gtk_container_add( GTK_CONTAINER(mis_transp_frame) , mis_transp_vbox );
  gtk_box_pack_start( GTK_BOX(mis_vbox) , mis_transp_frame , FALSE , FALSE , 0 );

  mis_transp_fake_rbt = gtk_radio_button_new_with_label( NULL ,
                          _("Fake transparency") );
  mis_transp_real_rbt = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(mis_transp_fake_rbt) ,
                          _("Real transparency (requires X Composite Ext.)") );
  g_object_set_data( G_OBJECT(mis_transp_fake_rbt) , "val" ,
                     GINT_TO_POINTER(AOSD_MISC_TRANSPARENCY_FAKE) );
  g_object_set_data( G_OBJECT(mis_transp_real_rbt) , "val" ,
                     GINT_TO_POINTER(AOSD_MISC_TRANSPARENCY_REAL) );
  gtk_box_pack_start( GTK_BOX(mis_transp_vbox) , mis_transp_fake_rbt , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(mis_transp_vbox) , mis_transp_real_rbt , TRUE , TRUE , 0 );

  mis_transp_status_hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 4 );
  mis_transp_status_frame = gtk_frame_new( NULL );
  gtk_container_set_border_width( GTK_CONTAINER(mis_transp_status_hbox) , 3 );
  gtk_container_add( GTK_CONTAINER(mis_transp_status_frame) , mis_transp_status_hbox );
  gtk_box_pack_start( GTK_BOX(mis_transp_vbox) , mis_transp_status_frame , TRUE , TRUE , 0 );

  mis_transp_status_img = gtk_image_new();
  gtk_misc_set_alignment( GTK_MISC(mis_transp_status_img) , 0.5 , 0 );
  mis_transp_status_label = gtk_label_new( "" );
  gtk_misc_set_alignment( GTK_MISC(mis_transp_status_label) , 0 , 0.5 );
  gtk_label_set_line_wrap( GTK_LABEL(mis_transp_status_label) , TRUE );
  gtk_box_pack_start( GTK_BOX(mis_transp_status_hbox) , mis_transp_status_img , FALSE , FALSE , 0 );
  gtk_box_pack_start( GTK_BOX(mis_transp_status_hbox) , mis_transp_status_label , TRUE , TRUE , 0 );
  g_object_set_data( G_OBJECT(mis_transp_status_hbox) , "img" , mis_transp_status_img );
  g_object_set_data( G_OBJECT(mis_transp_status_hbox) , "label" , mis_transp_status_label );

#ifdef HAVE_XCOMPOSITE
  g_signal_connect( G_OBJECT(mis_transp_real_rbt) , "toggled" ,
    G_CALLBACK(aosd_cb_configure_misc_transp_real_clicked) , mis_transp_status_hbox );

  /* check if the composite extension is loaded */
  if ( aosd_osd_check_composite_ext() )
  {
    if ( cfg->osd->misc.transparency_mode == AOSD_MISC_TRANSPARENCY_FAKE )
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(mis_transp_fake_rbt) , TRUE );
    else
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(mis_transp_real_rbt) , TRUE );
  }
  else
  {
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(mis_transp_fake_rbt) , TRUE );
    gtk_widget_set_sensitive( GTK_WIDGET(mis_transp_real_rbt) , FALSE );
    gtk_image_set_from_icon_name( GTK_IMAGE(mis_transp_status_img) ,
      "dialog-error" , GTK_ICON_SIZE_MENU );
    gtk_label_set_text( GTK_LABEL(mis_transp_status_label) , _("Composite extension not loaded") );
    gtk_widget_set_sensitive( GTK_WIDGET(mis_transp_status_hbox) , FALSE );
  }
#else
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(mis_transp_fake_rbt) , TRUE );
  gtk_widget_set_sensitive( GTK_WIDGET(mis_transp_real_rbt) , FALSE );
  gtk_image_set_from_icon_name( GTK_IMAGE(mis_transp_status_img) ,
    "dialog-error" , GTK_ICON_SIZE_MENU );
  gtk_label_set_text( GTK_LABEL(mis_transp_status_label) , _("Composite extension not available") );
  gtk_widget_set_sensitive( GTK_WIDGET(mis_transp_status_hbox) , FALSE );
#endif

  aosd_callback_list_add( cb_list , mis_transp_vbox , aosd_cb_configure_misc_transp_commit );

  return mis_vbox;
}


static void
aosd_cb_configure_test ( gpointer cfg_win )
{
  gchar *markup_message = NULL;
  aosd_cfg_t *cfg = aosd_cfg_new();
  GList *cb_list = g_object_get_data( G_OBJECT(cfg_win) , "cblist" );
  aosd_callback_list_run( cb_list , cfg );
  cfg->set = TRUE;
  markup_message = g_markup_printf_escaped(
    _("<span font_desc='%s'>Audacious OSD</span>") , cfg->osd->text.fonts_name[0] );
  aosd_osd_shutdown(); /* stop any displayed osd */
  aosd_osd_cleanup(); /* just in case it's active */
  aosd_osd_init( cfg->osd->misc.transparency_mode );
  aosd_osd_display( markup_message , cfg->osd , TRUE );
  g_free( markup_message );
  aosd_cfg_delete( cfg );
}


static void
aosd_cb_configure_cancel ( gpointer cfg_win )
{
  GList *cb_list = g_object_get_data( G_OBJECT(cfg_win) , "cblist" );
  aosd_callback_list_free( cb_list );
  aosd_osd_shutdown(); /* stop any displayed osd */
  aosd_osd_cleanup(); /* just in case it's active */
  if ( plugin_is_active == TRUE )
    aosd_osd_init( global_config->osd->misc.transparency_mode );
  gtk_widget_destroy( GTK_WIDGET(cfg_win) );
}


static void
aosd_cb_configure_ok ( gpointer cfg_win )
{
  //gchar *markup_message = NULL;
  aosd_cfg_t *cfg = aosd_cfg_new();
  GList *cb_list = g_object_get_data( G_OBJECT(cfg_win) , "cblist" );
  aosd_callback_list_run( cb_list , cfg );
  cfg->set = TRUE;
  aosd_osd_shutdown(); /* stop any displayed osd */
  aosd_osd_cleanup(); /* just in case it's active */

  if ( global_config != NULL )
  {
    /* plugin is active */
    aosd_trigger_stop( &global_config->osd->trigger ); /* stop triggers */
    aosd_cfg_delete( global_config ); /* delete old global_config */
    global_config = cfg; /* put the new one */
    aosd_cfg_save( cfg ); /* save the new configuration on config file */
    aosd_osd_init( cfg->osd->misc.transparency_mode ); /* restart osd */
    aosd_trigger_start( &cfg->osd->trigger ); /* restart triggers */
  }
  else
  {
    /* plugin is not active */
    aosd_cfg_save( cfg ); /* save the new configuration on config file */
  }
  aosd_callback_list_free( cb_list );
  gtk_widget_destroy( GTK_WIDGET(cfg_win) );
}


void
aosd_ui_configure ( aosd_cfg_t * cfg )
{
  static GtkWidget *cfg_win = NULL;
  GtkWidget *cfg_vbox;
  GtkWidget *cfg_nb;
  GtkWidget *cfg_bbar_hbbox;
  GtkWidget *cfg_bbar_bt_ok, *cfg_bbar_bt_test, *cfg_bbar_bt_cancel;
  GtkWidget *cfg_position_widget;
  GtkWidget *cfg_animation_widget;
  GtkWidget *cfg_text_widget;
  GtkWidget *cfg_decoration_widget;
  GtkWidget *cfg_trigger_widget;
  GdkGeometry cfg_win_hints;
  GList *cb_list = NULL; /* list of custom callbacks */

  if ( cfg_win != NULL )
    gtk_window_present( GTK_WINDOW(cfg_win) );

  cfg_win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_type_hint( GTK_WINDOW(cfg_win), GDK_WINDOW_TYPE_HINT_DIALOG );
  gtk_window_set_title( GTK_WINDOW(cfg_win) , _("Audacious OSD - configuration") );
  gtk_container_set_border_width( GTK_CONTAINER(cfg_win), 10 );
  g_signal_connect( G_OBJECT(cfg_win) , "destroy" ,
                    G_CALLBACK(gtk_widget_destroyed) , &cfg_win );
  cfg_win_hints.min_width = -1;
  cfg_win_hints.min_height = 350;
  gtk_window_set_geometry_hints( GTK_WINDOW(cfg_win) , GTK_WIDGET(cfg_win) ,
                                 &cfg_win_hints , GDK_HINT_MIN_SIZE );

  cfg_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , FALSE );
  gtk_container_add( GTK_CONTAINER(cfg_win) , cfg_vbox );

  cfg_nb = gtk_notebook_new();
  gtk_notebook_set_tab_pos( GTK_NOTEBOOK(cfg_nb) , GTK_POS_TOP );
  gtk_box_pack_start( GTK_BOX(cfg_vbox) , cfg_nb , TRUE , TRUE , 0 );

  gtk_box_pack_start( GTK_BOX(cfg_vbox) , gtk_separator_new(GTK_ORIENTATION_HORIZONTAL) , FALSE , FALSE , 4 );

  cfg_bbar_hbbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout( GTK_BUTTON_BOX(cfg_bbar_hbbox) , GTK_BUTTONBOX_START );
  gtk_box_pack_start( GTK_BOX(cfg_vbox) , cfg_bbar_hbbox , FALSE , FALSE , 0 );
  cfg_bbar_bt_test = audgui_button_new (_("_Test"), "media-playback-start", NULL, NULL);
  gtk_container_add( GTK_CONTAINER(cfg_bbar_hbbox) , cfg_bbar_bt_test );
  gtk_button_box_set_child_secondary( GTK_BUTTON_BOX(cfg_bbar_hbbox) , cfg_bbar_bt_test , FALSE );
  cfg_bbar_bt_cancel = audgui_button_new (_("_Cancel"), "process-stop", NULL, NULL);
  gtk_container_add( GTK_CONTAINER(cfg_bbar_hbbox) , cfg_bbar_bt_cancel );
  gtk_button_box_set_child_secondary( GTK_BUTTON_BOX(cfg_bbar_hbbox) , cfg_bbar_bt_cancel , TRUE );
  cfg_bbar_bt_ok = audgui_button_new (_("_Set"), "system-run", NULL, NULL);
  gtk_container_add( GTK_CONTAINER(cfg_bbar_hbbox) , cfg_bbar_bt_ok );
  gtk_button_box_set_child_secondary( GTK_BUTTON_BOX(cfg_bbar_hbbox) , cfg_bbar_bt_ok , TRUE );

  /* add POSITION page */
  cfg_position_widget = aosd_ui_configure_position( cfg , &cb_list );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_position_widget , gtk_label_new( _("Position") ) );

  /* add ANIMATION page */
  cfg_animation_widget = aosd_ui_configure_animation( cfg , &cb_list );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_animation_widget , gtk_label_new( _("Animation") ) );

  /* add TEXT page */
  cfg_text_widget = aosd_ui_configure_text( cfg , &cb_list );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_text_widget , gtk_label_new( _("Text") ) );

  /* add DECORATION page */
  cfg_decoration_widget = aosd_ui_configure_decoration( cfg , &cb_list );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_decoration_widget , gtk_label_new( _("Decoration") ) );

  /* add TRIGGER page */
  cfg_trigger_widget = aosd_ui_configure_trigger( cfg , &cb_list );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_trigger_widget , gtk_label_new( _("Trigger") ) );

  /* add MISC page */
  cfg_trigger_widget = aosd_ui_configure_misc( cfg , &cb_list );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_trigger_widget , gtk_label_new( _("Misc") ) );

  g_object_set_data( G_OBJECT(cfg_win) , "cblist" , cb_list );

  g_signal_connect_swapped( G_OBJECT(cfg_bbar_bt_test) , "clicked" ,
                            G_CALLBACK(aosd_cb_configure_test) , cfg_win );
  g_signal_connect_swapped( G_OBJECT(cfg_bbar_bt_cancel) , "clicked" ,
                            G_CALLBACK(aosd_cb_configure_cancel) , cfg_win );
  g_signal_connect_swapped( G_OBJECT(cfg_bbar_bt_ok) , "clicked" ,
                            G_CALLBACK(aosd_cb_configure_ok) , cfg_win );

  gtk_widget_show_all( cfg_win );
}
