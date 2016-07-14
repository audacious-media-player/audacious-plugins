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
#include <libaudcore/index.h>
#include <libaudcore/preferences.h>

#include "aosd.h"
#include "aosd_style.h"
#include "aosd_trigger.h"
#include "aosd_cfg.h"
#include "aosd_osd.h"


static void chooser_get_aosd_color (GtkColorButton * chooser, aosd_color_t * color)
{
  GdkColor gdk_color;
  gtk_color_button_get_color (chooser, & gdk_color);

  color->red = gdk_color.red;
  color->green = gdk_color.green;
  color->blue = gdk_color.blue;
  color->alpha = gtk_color_button_get_alpha (chooser);
}


static void chooser_set_aosd_color (GtkColorButton * chooser, const aosd_color_t * color)
{
  GdkColor gdk_color = {0, (uint16_t) color->red, (uint16_t) color->green, (uint16_t) color->blue};

  gtk_color_button_set_color (chooser, & gdk_color);
  gtk_color_button_set_use_alpha (chooser, true);
  gtk_color_button_set_alpha (chooser, color->alpha);
}


/*************************************************************/
/* small callback system used by the configuration interface */
typedef void (*aosd_ui_cb_func_t)( GtkWidget * , aosd_cfg_t * );

typedef struct
{
  GtkWidget * widget;
  aosd_ui_cb_func_t func;
}
aosd_ui_cb_t;

static Index<aosd_ui_cb_t> aosd_cb_list;

static void
aosd_callback_list_run ( aosd_cfg_t * cfg )
{
  for (const aosd_ui_cb_t & cb : aosd_cb_list)
    cb.func (cb.widget, cfg);
}
/*************************************************************/



static gboolean
aosd_cb_configure_position_expose ( GtkWidget * darea ,
                                    GdkEventExpose * event ,
                                    void * coord_gp )
{
  int coord = GPOINTER_TO_INT(coord_gp);

  cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (darea));
  cairo_set_source_rgb ( cr , 0 , 0 , 0 );
  cairo_rectangle ( cr , (coord % 3) * 10 , (coord / 3) * 16 , 20 , 8 );
  cairo_fill ( cr );
  cairo_destroy (cr);

  return false;
}


static void
aosd_cb_configure_position_placement_commit ( GtkWidget * grid , aosd_cfg_t * cfg )
{
  GList *placbt_list = gtk_container_get_children( GTK_CONTAINER(grid) );

  for ( GList *iter = placbt_list; iter != nullptr; iter = iter->next )
  {
    GtkWidget *placbt = (GtkWidget *) iter->data;

    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(placbt) ) == true )
    {
      cfg->position.placement = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(placbt),"value"));
      break;
    }
  }

  g_list_free( placbt_list );
}


static void
aosd_cb_configure_position_offset_commit ( GtkWidget * grid , aosd_cfg_t * cfg )
{
  cfg->position.offset_x = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(grid),"offx")) );
  cfg->position.offset_y = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(grid),"offy")) );
}


static void
aosd_cb_configure_position_maxsize_commit ( GtkWidget * grid , aosd_cfg_t * cfg )
{
  cfg->position.maxsize_width = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(grid),"maxsize_width")) );
}


static void
aosd_cb_configure_position_multimon_commit ( GtkWidget * combo , aosd_cfg_t * cfg )
{
  int active = gtk_combo_box_get_active( GTK_COMBO_BOX(combo) );
  cfg->position.multimon_id = ( active > -1 ) ? (active - 1) : -1;
}


static GtkWidget *
aosd_ui_configure_position ( aosd_cfg_t * cfg )
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
  int monitors_num = gdk_screen_get_n_monitors( gdk_screen_get_default() );
  int i = 0;

  pos_vbox = gtk_vbox_new( false , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(pos_vbox) , 6 );

  pos_placement_frame = gtk_frame_new( _("Placement") );
  pos_placement_hbox = gtk_hbox_new( false , 0 );
  gtk_container_set_border_width( GTK_CONTAINER(pos_placement_hbox) , 6 );
  gtk_container_add( GTK_CONTAINER(pos_placement_frame) , pos_placement_hbox );
  gtk_box_pack_start( GTK_BOX(pos_vbox) , pos_placement_frame , false , false , 0 );

  pos_placement_grid = gtk_table_new (0, 0, false);
  for ( i = 0 ; i < 9 ; i++ )
  {
    if ( i == 0 )
      pos_placement_bt[i] = gtk_radio_button_new( nullptr );
    else
      pos_placement_bt[i] = gtk_radio_button_new_from_widget( GTK_RADIO_BUTTON(pos_placement_bt[0]) );
    gtk_toggle_button_set_mode( GTK_TOGGLE_BUTTON(pos_placement_bt[i]) , false );
    pos_placement_bt_darea[i] = gtk_drawing_area_new();
    gtk_widget_set_size_request( pos_placement_bt_darea[i] , 40 , 40 );
    gtk_container_add( GTK_CONTAINER(pos_placement_bt[i]) , pos_placement_bt_darea[i] );
    g_signal_connect( G_OBJECT(pos_placement_bt_darea[i]) , "expose-event" ,
                      G_CALLBACK(aosd_cb_configure_position_expose) , GINT_TO_POINTER(i) );
    gtk_table_attach_defaults( GTK_TABLE(pos_placement_grid) , pos_placement_bt[i] ,
                               (i % 3) , (i % 3) + 1 , (i / 3) , (i / 3) + 1 );
    g_object_set_data( G_OBJECT(pos_placement_bt[i]) , "value" , GINT_TO_POINTER(i+1) );
    if ( cfg->position.placement == (i+1) )
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(pos_placement_bt[i]) , true );
  }
  gtk_box_pack_start( GTK_BOX(pos_placement_hbox) , pos_placement_grid , false , false , 0 );
  aosd_cb_list.append( pos_placement_grid , aosd_cb_configure_position_placement_commit );

  gtk_box_pack_start( GTK_BOX(pos_placement_hbox) , gtk_vseparator_new() , false , false , 6 );

  pos_offset_grid = gtk_table_new (0, 0, false);
  gtk_table_set_row_spacings( GTK_TABLE(pos_offset_grid) , 4 );
  gtk_table_set_col_spacings( GTK_TABLE(pos_offset_grid) , 4 );
  pos_offset_x_label = gtk_label_new( _( "Relative X offset:" ) );
  gtk_misc_set_alignment( GTK_MISC(pos_offset_x_label) , 0 , 0.5 );
  gtk_table_attach_defaults( GTK_TABLE(pos_offset_grid) , pos_offset_x_label , 0 , 1 , 0 , 1 );
  pos_offset_x_spinbt = gtk_spin_button_new_with_range( -9999 , 9999 , 1 );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(pos_offset_x_spinbt) , cfg->position.offset_x );
  gtk_table_attach_defaults( GTK_TABLE(pos_offset_grid) , pos_offset_x_spinbt , 1 , 2 , 0 , 1 );
  g_object_set_data( G_OBJECT(pos_offset_grid) , "offx" , pos_offset_x_spinbt );
  pos_offset_y_label = gtk_label_new( _( "Relative Y offset:" ) );
  gtk_misc_set_alignment( GTK_MISC(pos_offset_y_label) , 0 , 0.5 );
  gtk_table_attach_defaults( GTK_TABLE(pos_offset_grid) , pos_offset_y_label , 0 , 1 , 1 , 2 );
  pos_offset_y_spinbt = gtk_spin_button_new_with_range( -9999 , 9999 , 1 );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(pos_offset_y_spinbt) , cfg->position.offset_y );
  gtk_table_attach_defaults( GTK_TABLE(pos_offset_grid) , pos_offset_y_spinbt , 1 , 2 , 1 , 2 );
  g_object_set_data( G_OBJECT(pos_offset_grid) , "offy" , pos_offset_y_spinbt );
  pos_maxsize_width_label = gtk_label_new( _("Max OSD width:") );
  gtk_misc_set_alignment( GTK_MISC(pos_maxsize_width_label) , 0 , 0.5 );
  gtk_table_attach_defaults( GTK_TABLE(pos_offset_grid) , pos_maxsize_width_label , 0 , 1 , 2 , 3 );
  pos_maxsize_width_spinbt = gtk_spin_button_new_with_range( 0 , 99999 , 1 );
  g_object_set_data( G_OBJECT(pos_offset_grid) , "maxsize_width" , pos_maxsize_width_spinbt );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(pos_maxsize_width_spinbt) , cfg->position.maxsize_width );
  gtk_table_attach_defaults( GTK_TABLE(pos_offset_grid) , pos_maxsize_width_spinbt , 1 , 2 , 2 , 3 );
  gtk_box_pack_start( GTK_BOX(pos_placement_hbox) , pos_offset_grid , false , false , 0 );
  aosd_cb_list.append( pos_offset_grid , aosd_cb_configure_position_offset_commit );
  aosd_cb_list.append( pos_offset_grid , aosd_cb_configure_position_maxsize_commit );

  pos_multimon_frame = gtk_frame_new( _("Multi-Monitor options") );
  pos_multimon_hbox = gtk_hbox_new( false , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(pos_multimon_hbox) , 6 );
  gtk_container_add( GTK_CONTAINER(pos_multimon_frame), pos_multimon_hbox );
  pos_multimon_label = gtk_label_new( _("Display OSD using:") );
  pos_multimon_combobox = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text ((GtkComboBoxText *) pos_multimon_combobox, _("all monitors"));
  for ( i = 0 ; i < monitors_num ; i++ )
  {
    char *mon_str = g_strdup_printf( _("monitor %i") , i + 1 );
    gtk_combo_box_text_append_text ((GtkComboBoxText *) pos_multimon_combobox, mon_str);
    g_free( mon_str );
  }
  gtk_combo_box_set_active( GTK_COMBO_BOX(pos_multimon_combobox) , (cfg->position.multimon_id + 1) );
  aosd_cb_list.append( pos_multimon_combobox , aosd_cb_configure_position_multimon_commit );
  gtk_box_pack_start( GTK_BOX(pos_multimon_hbox) , pos_multimon_label , false , false , 0 );
  gtk_box_pack_start( GTK_BOX(pos_multimon_hbox) , pos_multimon_combobox , false , false , 0 );
  gtk_box_pack_start( GTK_BOX(pos_vbox) , pos_multimon_frame , false , false , 0 );

  return pos_vbox;
}


static GtkWidget *
aosd_ui_configure_animation_timing ( char * label_string )
{
  GtkWidget *hbox, *desc_label, *spinbt;
  hbox = gtk_hbox_new( false , 4 );
  desc_label = gtk_label_new( label_string );
  spinbt = gtk_spin_button_new_with_range( 0 , 99999 , 1 );
  gtk_box_pack_start( GTK_BOX(hbox) , desc_label , false , false , 0 );
  gtk_box_pack_start( GTK_BOX(hbox) , spinbt , false , false , 0 );
  g_object_set_data( G_OBJECT(hbox) , "spinbt" , spinbt );
  return hbox;
}


static void
aosd_cb_configure_animation_timing_commit ( GtkWidget * timing_hbox , aosd_cfg_t * cfg )
{
  cfg->animation.timing_display = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(timing_hbox),"display")) );
  cfg->animation.timing_fadein = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(timing_hbox),"fadein")) );
  cfg->animation.timing_fadeout = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(timing_hbox),"fadeout")) );
}


static GtkWidget *
aosd_ui_configure_animation ( aosd_cfg_t * cfg )
{
  GtkWidget *ani_vbox;
  GtkWidget *ani_timing_frame, *ani_timing_hbox;
  GtkWidget *ani_timing_fadein_widget, *ani_timing_fadeout_widget, *ani_timing_stay_widget;
  GtkSizeGroup *sizegroup;

  ani_vbox = gtk_vbox_new( false , 0 );
  gtk_container_set_border_width( GTK_CONTAINER(ani_vbox) , 6 );

  ani_timing_hbox = gtk_hbox_new( false , 0 );
  ani_timing_frame = gtk_frame_new( _("Timing (ms)") );
  gtk_container_set_border_width( GTK_CONTAINER(ani_timing_hbox) , 6 );
  gtk_container_add( GTK_CONTAINER(ani_timing_frame) , ani_timing_hbox );
  gtk_box_pack_start( GTK_BOX(ani_vbox) , ani_timing_frame , false , false , 0 );

  ani_timing_stay_widget = aosd_ui_configure_animation_timing( _("Display:") );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(g_object_get_data(
    G_OBJECT(ani_timing_stay_widget),"spinbt")) , cfg->animation.timing_display );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , ani_timing_stay_widget , true , true , 0 );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , gtk_vseparator_new() , false , false , 4 );
  ani_timing_fadein_widget = aosd_ui_configure_animation_timing( _("Fade in:") );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(g_object_get_data(
    G_OBJECT(ani_timing_fadein_widget),"spinbt")) , cfg->animation.timing_fadein );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , ani_timing_fadein_widget , true , true , 0 );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , gtk_vseparator_new() , false , false , 4 );
  ani_timing_fadeout_widget = aosd_ui_configure_animation_timing( _("Fade out:") );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(g_object_get_data(
    G_OBJECT(ani_timing_fadeout_widget),"spinbt")) , cfg->animation.timing_fadeout );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , ani_timing_fadeout_widget , true , true , 0 );
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
  aosd_cb_list.append( ani_timing_hbox , aosd_cb_configure_animation_timing_commit );

  return ani_vbox;
}


static void
aosd_cb_configure_text_font_shadow_toggle ( GtkToggleButton * shadow_togglebt ,
                                            void * shadow_colorbt )
{
  if ( gtk_toggle_button_get_active( shadow_togglebt ) == true )
    gtk_widget_set_sensitive( GTK_WIDGET(shadow_colorbt) , true );
  else
    gtk_widget_set_sensitive( GTK_WIDGET(shadow_colorbt) , false );
}


static void
aosd_cb_configure_text_font_commit ( GtkWidget * fontbt , aosd_cfg_t * cfg )
{
  int fontnum = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(fontbt) , "fontnum" ));
  GtkColorButton * chooser;

  cfg->text.fonts_name[fontnum] =
   String (gtk_font_button_get_font_name (GTK_FONT_BUTTON (fontbt)));

  cfg->text.fonts_draw_shadow[fontnum] = gtk_toggle_button_get_active(
    GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(fontbt),"use_shadow")) );

  chooser = (GtkColorButton *) g_object_get_data ((GObject *) fontbt, "color");
  chooser_get_aosd_color (chooser, & cfg->text.fonts_color[fontnum]);

  chooser = (GtkColorButton *) g_object_get_data ((GObject *) fontbt, "shadow_color");
  chooser_get_aosd_color (chooser, & cfg->text.fonts_shadow_color[fontnum]);
}


static GtkWidget *
aosd_ui_configure_text ( aosd_cfg_t * cfg )
{
  GtkWidget *tex_vbox;
  GtkWidget *tex_font_grid, *tex_font_frame;
  GtkWidget *tex_font_label[3], *tex_font_fontbt[3];
  GtkWidget *tex_font_colorbt[3], *tex_font_shadow_togglebt[3];
  GtkWidget *tex_font_shadow_colorbt[3];
  int i = 0;

  tex_vbox = gtk_vbox_new( false , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(tex_vbox) , 6 );

  tex_font_frame = gtk_frame_new( _("Fonts") );
  tex_font_grid = gtk_table_new (0, 0, false);
  gtk_container_set_border_width( GTK_CONTAINER(tex_font_grid) , 6 );
  gtk_table_set_row_spacings( GTK_TABLE(tex_font_grid) , 4 );
  gtk_table_set_col_spacings( GTK_TABLE(tex_font_grid) , 4 );
  for ( i = 0 ; i < AOSD_TEXT_FONTS_NUM ; i++ )
  {
    char *label_str = g_strdup_printf( _("Font %i:") , i+1 );
    tex_font_label[i] = gtk_label_new( label_str );
    g_free( label_str );
    tex_font_fontbt[i] = gtk_font_button_new();
    gtk_font_button_set_show_style( GTK_FONT_BUTTON(tex_font_fontbt[i]) , true );
    gtk_font_button_set_show_size( GTK_FONT_BUTTON(tex_font_fontbt[i]) , true );
    gtk_font_button_set_use_font( GTK_FONT_BUTTON(tex_font_fontbt[i]) , false );
    gtk_font_button_set_use_size( GTK_FONT_BUTTON(tex_font_fontbt[i]) , false );
    gtk_font_button_set_font_name( GTK_FONT_BUTTON(tex_font_fontbt[i]) , cfg->text.fonts_name[i] );

    tex_font_colorbt[i] = gtk_color_button_new ();
    chooser_set_aosd_color ((GtkColorButton *) tex_font_colorbt[i],
     & cfg->text.fonts_color[i]);

    tex_font_shadow_togglebt[i] = gtk_toggle_button_new_with_label( _("Shadow") );
    gtk_toggle_button_set_mode( GTK_TOGGLE_BUTTON(tex_font_shadow_togglebt[i]) , false );

    tex_font_shadow_colorbt[i] = gtk_color_button_new ();
    chooser_set_aosd_color ((GtkColorButton *) tex_font_shadow_colorbt[i],
     & cfg->text.fonts_shadow_color[i]);

    gtk_widget_set_sensitive( tex_font_shadow_colorbt[i] , false );
    g_signal_connect( G_OBJECT(tex_font_shadow_togglebt[i]) , "toggled" ,
                      G_CALLBACK(aosd_cb_configure_text_font_shadow_toggle) ,
                      tex_font_shadow_colorbt[i] );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(tex_font_shadow_togglebt[i]) ,
      cfg->text.fonts_draw_shadow[i] );
    gtk_table_attach_defaults( GTK_TABLE(tex_font_grid) , tex_font_label[i] , 0 , 1 , i , i + 1 );
    gtk_table_attach_defaults( GTK_TABLE(tex_font_grid) , tex_font_fontbt[i] , 1 , 2 , i , i + 1 );
    gtk_table_attach_defaults( GTK_TABLE(tex_font_grid) , tex_font_colorbt[i] , 2 , 3 , i , i + 1 );
    gtk_table_attach_defaults( GTK_TABLE(tex_font_grid) , tex_font_shadow_togglebt[i] , 3 , 4 , i , i + 1 );
    gtk_table_attach_defaults( GTK_TABLE(tex_font_grid) , tex_font_shadow_colorbt[i] , 4 , 5 , i , i + 1 );
    g_object_set_data( G_OBJECT(tex_font_fontbt[i]) , "fontnum" , GINT_TO_POINTER(i) );
    g_object_set_data( G_OBJECT(tex_font_fontbt[i]) , "color" , tex_font_colorbt[i] );
    g_object_set_data( G_OBJECT(tex_font_fontbt[i]) , "use_shadow" , tex_font_shadow_togglebt[i] );
    g_object_set_data( G_OBJECT(tex_font_fontbt[i]) , "shadow_color" , tex_font_shadow_colorbt[i] );
    aosd_cb_list.append( tex_font_fontbt[i] , aosd_cb_configure_text_font_commit );
  }
  gtk_container_add( GTK_CONTAINER(tex_font_frame) , tex_font_grid );
  gtk_box_pack_start( GTK_BOX(tex_vbox) , tex_font_frame , false , false , 0 );

  return tex_vbox;
}


static void
aosd_cb_configure_decoration_style_commit ( GtkWidget * lv , aosd_cfg_t * cfg )
{
  GtkTreeSelection *sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(lv) );
  GtkTreeModel *model;
  GtkTreeIter iter;

  if ( gtk_tree_selection_get_selected( sel , &model , &iter ) == true )
  {
    int deco_code = 0;
    gtk_tree_model_get( model , &iter , 1 , &deco_code , -1 );
    cfg->decoration.code = deco_code;
  }
}


static void
aosd_cb_configure_decoration_color_commit ( GtkWidget * colorbt , aosd_cfg_t * cfg )
{
  aosd_color_t color;
  chooser_get_aosd_color ((GtkColorButton *) colorbt, & color);

  int colnum = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(colorbt) , "colnum" ) );
  cfg->decoration.colors[colnum] = color;
}


static GtkWidget *
aosd_ui_configure_decoration ( aosd_cfg_t * cfg )
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
  int colors_max_num = 0, i = 0;

  dec_hbox = gtk_hbox_new( false , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(dec_hbox) , 6 );

  /* decoration style model
     ---------------------------------------------
     G_TYPE_STRING -> decoration description
     G_TYPE_INT -> decoration code
     G_TYPE_INT -> number of user-definable colors
     ---------------------------------------------
  */
  dec_rstyle_store = gtk_list_store_new( 3 , G_TYPE_STRING , G_TYPE_INT , G_TYPE_INT );
  for ( i = 0 ; i < AOSD_NUM_DECO_STYLES ; i++ )
  {
    int colors_num = aosd_deco_style_get_numcol( i );
    if ( colors_num > colors_max_num )
      colors_max_num = colors_num;
    gtk_list_store_append( dec_rstyle_store , &iter );
    gtk_list_store_set( dec_rstyle_store , &iter ,
      0 , _(aosd_deco_style_get_desc( i )) ,
      1 , i , 2 , colors_num , -1 );
    if ( i == cfg->decoration.code )
      iter_sel = iter;
  }

  dec_rstyle_lv_frame = gtk_frame_new( nullptr );
  dec_rstyle_lv = gtk_tree_view_new_with_model( GTK_TREE_MODEL(dec_rstyle_store) );
  g_object_unref( dec_rstyle_store );
  dec_rstyle_lv_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(dec_rstyle_lv) );
  gtk_tree_selection_set_mode( dec_rstyle_lv_sel , GTK_SELECTION_BROWSE );

  dec_rstyle_lv_rndr_text = gtk_cell_renderer_text_new();
  dec_rstyle_lv_col_desc = gtk_tree_view_column_new_with_attributes(
    _("Render Style") , dec_rstyle_lv_rndr_text , "text" , 0 , nullptr );
  gtk_tree_view_append_column( GTK_TREE_VIEW(dec_rstyle_lv), dec_rstyle_lv_col_desc );
  dec_rstyle_lv_sw = gtk_scrolled_window_new( nullptr , nullptr );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(dec_rstyle_lv_sw) ,
                                  GTK_POLICY_NEVER , GTK_POLICY_ALWAYS );
  gtk_container_add( GTK_CONTAINER(dec_rstyle_lv_sw) , dec_rstyle_lv );
  gtk_container_add( GTK_CONTAINER(dec_rstyle_lv_frame) , dec_rstyle_lv_sw );

  gtk_tree_selection_select_iter( dec_rstyle_lv_sel , &iter_sel );
  gtk_box_pack_start( GTK_BOX(dec_hbox) , dec_rstyle_lv_frame , false , false , 0 );
  aosd_cb_list.append( dec_rstyle_lv , aosd_cb_configure_decoration_style_commit );

  dec_rstyle_hbox = gtk_vbox_new( false , 4 );
  gtk_box_pack_start( GTK_BOX(dec_hbox) , dec_rstyle_hbox , true , true , 0 );

  /* in colors_max_num now there's the maximum number of colors used by decoration styles */
  dec_rstyleopts_frame = gtk_frame_new( _("Colors") );
  dec_rstyleopts_grid = gtk_table_new (0, 0, false);
  gtk_container_set_border_width( GTK_CONTAINER(dec_rstyleopts_grid) , 6 );
  gtk_table_set_row_spacings( GTK_TABLE(dec_rstyleopts_grid) , 4 );
  gtk_table_set_col_spacings( GTK_TABLE(dec_rstyleopts_grid) , 8 );
  gtk_container_add( GTK_CONTAINER(dec_rstyleopts_frame) , dec_rstyleopts_grid );
  for ( i = 0 ; i < colors_max_num ; i++ )
  {
    GtkWidget *hbox, *label;
    char *label_str = nullptr;
    hbox = gtk_hbox_new( false , 4 );
    label_str = g_strdup_printf( _("Color %i:") , i+1 );
    label = gtk_label_new( label_str );
    g_free( label_str );

    GtkWidget * colorbt = gtk_color_button_new ();
    chooser_set_aosd_color ((GtkColorButton *) colorbt, & cfg->decoration.colors[i]);

    gtk_box_pack_start( GTK_BOX(hbox) , label , false , false , 0 );
    gtk_box_pack_start( GTK_BOX(hbox) , colorbt , false , false , 0 );
    gtk_table_attach_defaults( GTK_TABLE(dec_rstyleopts_grid) , hbox , (i % 3) , (i % 3) + 1, (i / 3) , (i / 3) + 1);
    g_object_set_data( G_OBJECT(colorbt) , "colnum" , GINT_TO_POINTER(i) );
    aosd_cb_list.append( colorbt , aosd_cb_configure_decoration_color_commit );
  }
  gtk_box_pack_start( GTK_BOX(dec_rstyle_hbox) , dec_rstyleopts_frame , false , false , 0 );

  return dec_hbox;
}


static void
aosd_cb_configure_trigger_lvchanged ( GtkTreeSelection *sel , void * nb )
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  if ( gtk_tree_selection_get_selected( sel , &model , &iter ) == true )
  {
    int page_num = 0;
    gtk_tree_model_get( model , &iter , 2 , &page_num , -1 );
    gtk_notebook_set_current_page( GTK_NOTEBOOK(nb) , page_num );
  }
}


static void
aosd_cb_configure_trigger_commit ( GtkWidget * cbt , aosd_cfg_t * cfg )
{
  if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(cbt) ) == true )
  {
    int value = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(cbt),"code"));
    cfg->trigger.enabled[value] = true;
  }
}


static GtkWidget *
aosd_ui_configure_trigger ( aosd_cfg_t * cfg )
{
  GtkWidget *tri_hbox;
  GtkWidget *tri_event_lv, *tri_event_lv_frame, *tri_event_lv_sw;
  GtkListStore *tri_event_store;
  GtkCellRenderer *tri_event_lv_rndr_text;
  GtkTreeViewColumn *tri_event_lv_col_desc;
  GtkTreeSelection *tri_event_lv_sel;
  GtkTreeIter iter;
  GtkWidget *tri_event_nb;
  int i = 0;

  tri_event_nb = gtk_notebook_new();
  gtk_notebook_set_tab_pos( GTK_NOTEBOOK(tri_event_nb) , GTK_POS_LEFT );
  gtk_notebook_set_show_tabs( GTK_NOTEBOOK(tri_event_nb) , false );
  gtk_notebook_set_show_border( GTK_NOTEBOOK(tri_event_nb) , false );

  tri_hbox = gtk_hbox_new( false , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(tri_hbox) , 6 );

  /* trigger model
     ---------------------------------------------
     G_TYPE_STRING -> trigger description
     G_TYPE_INT -> trigger code
     G_TYPE_INT -> gtk notebook page number
     ---------------------------------------------
  */
  tri_event_store = gtk_list_store_new( 3 , G_TYPE_STRING , G_TYPE_INT , G_TYPE_INT );
  for ( i = 0 ; i < AOSD_NUM_TRIGGERS ; i ++ )
  {
    GtkWidget *frame, *vbox, *label, *checkbt;
    gtk_list_store_append( tri_event_store , &iter );
    gtk_list_store_set( tri_event_store , &iter ,
      0 , _(aosd_trigger_get_name( i )) ,
      1 , i , 2 , i , -1 );
    vbox = gtk_vbox_new( false , 0 );
    gtk_container_set_border_width( GTK_CONTAINER(vbox) , 6 );
    label = gtk_label_new( _(aosd_trigger_get_desc( i )) );
    gtk_label_set_line_wrap( GTK_LABEL(label) , true );
    gtk_label_set_max_width_chars( GTK_LABEL(label), 40 );
    gtk_misc_set_alignment( GTK_MISC(label) , 0.0 , 0.0 );
    checkbt = gtk_check_button_new_with_label( _("Enable trigger") );
    if ( cfg->trigger.enabled[i] )
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(checkbt) , true );
    else
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(checkbt) , false );
    gtk_box_pack_start( GTK_BOX(vbox) , checkbt , false , false , 0 );
    gtk_box_pack_start( GTK_BOX(vbox) , gtk_hseparator_new() , false , false , 4 );
    gtk_box_pack_start( GTK_BOX(vbox) , label , false , false , 0 );
    frame = gtk_frame_new( nullptr );
    gtk_container_add( GTK_CONTAINER(frame) , vbox );
    gtk_notebook_append_page( GTK_NOTEBOOK(tri_event_nb) , frame , nullptr );
    g_object_set_data( G_OBJECT(checkbt) , "code" , GINT_TO_POINTER(i) );
    aosd_cb_list.append( checkbt , aosd_cb_configure_trigger_commit );
  }

  tri_event_lv_frame = gtk_frame_new( nullptr );
  tri_event_lv = gtk_tree_view_new_with_model( GTK_TREE_MODEL(tri_event_store) );
  g_object_unref( tri_event_store );
  tri_event_lv_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(tri_event_lv) );
  gtk_tree_selection_set_mode( tri_event_lv_sel , GTK_SELECTION_BROWSE );
  g_signal_connect( G_OBJECT(tri_event_lv_sel) , "changed" ,
                    G_CALLBACK(aosd_cb_configure_trigger_lvchanged) , tri_event_nb );
  if ( gtk_tree_model_get_iter_first( GTK_TREE_MODEL(tri_event_store) , &iter ) == true )
    gtk_tree_selection_select_iter( tri_event_lv_sel , &iter );

  tri_event_lv_rndr_text = gtk_cell_renderer_text_new();
  tri_event_lv_col_desc = gtk_tree_view_column_new_with_attributes(
    _("Event") , tri_event_lv_rndr_text , "text" , 0 , nullptr );
  gtk_tree_view_append_column( GTK_TREE_VIEW(tri_event_lv), tri_event_lv_col_desc );
  tri_event_lv_sw = gtk_scrolled_window_new( nullptr , nullptr );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(tri_event_lv_sw) ,
                                  GTK_POLICY_NEVER , GTK_POLICY_ALWAYS );
  gtk_container_add( GTK_CONTAINER(tri_event_lv_sw) , tri_event_lv );
  gtk_container_add( GTK_CONTAINER(tri_event_lv_frame) , tri_event_lv_sw );
  gtk_tree_selection_select_iter( tri_event_lv_sel , &iter );

  gtk_box_pack_start( GTK_BOX(tri_hbox) , tri_event_lv_frame , false , false , 0 );

  gtk_box_pack_start( GTK_BOX(tri_hbox) , tri_event_nb , true , true , 0 );

  return tri_hbox;
}


static void
aosd_cb_configure_misc_transp_real_clicked ( GtkToggleButton * real_rbt , void * status_hbox )
{
  GtkWidget *img = (GtkWidget *) g_object_get_data( G_OBJECT(status_hbox) , "img" );
  GtkWidget *label = (GtkWidget *) g_object_get_data( G_OBJECT(status_hbox) , "label" );
  if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(real_rbt) ) )
  {
    if ( aosd_osd_check_composite_mgr() )
    {
      gtk_image_set_from_icon_name( GTK_IMAGE(img) , "face-smile" , GTK_ICON_SIZE_MENU );
      gtk_label_set_text( GTK_LABEL(label) , _("Composite manager detected") );
      gtk_widget_set_sensitive( GTK_WIDGET(status_hbox) , true );
    }
    else
    {
      gtk_image_set_from_icon_name( GTK_IMAGE(img) , "dialog-warning" , GTK_ICON_SIZE_MENU );
      gtk_label_set_text( GTK_LABEL(label) ,
        _("Composite manager not detected;\nunless you know that you have one running, "
          "please activate a composite manager otherwise the OSD won't work properly") );
      gtk_widget_set_sensitive( GTK_WIDGET(status_hbox) , true );
    }
  }
  else
  {
    gtk_image_set_from_icon_name( GTK_IMAGE(img) , "dialog-information" , GTK_ICON_SIZE_MENU );
    gtk_label_set_text( GTK_LABEL(label) , _("Composite manager not required for fake transparency") );
    gtk_widget_set_sensitive( GTK_WIDGET(status_hbox) , false );
  }
}


static void
aosd_cb_configure_misc_transp_commit ( GtkWidget * mis_transp_vbox , aosd_cfg_t * cfg )
{
  GList *child_list = gtk_container_get_children( GTK_CONTAINER(mis_transp_vbox) );

  for ( GList *iter = child_list; iter != nullptr; iter = iter->next )
  {
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(iter->data) ) )
    {
      cfg->misc.transparency_mode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(iter->data), "val"));
      break;
    }
  }

  g_list_free( child_list );
}


static GtkWidget *
aosd_ui_configure_misc ( aosd_cfg_t * cfg )
{
  GtkWidget *mis_vbox;
  GtkWidget *mis_transp_frame, *mis_transp_vbox;
  GtkWidget *mis_transp_fake_rbt, *mis_transp_real_rbt;
  GtkWidget *mis_transp_status_frame, *mis_transp_status_hbox;
  GtkWidget *mis_transp_status_img, *mis_transp_status_label;

  mis_vbox = gtk_vbox_new( false , 0 );
  gtk_container_set_border_width( GTK_CONTAINER(mis_vbox) , 6 );

  mis_transp_vbox = gtk_vbox_new( false , 0 );
  mis_transp_frame = gtk_frame_new( _("Transparency") );
  gtk_container_set_border_width( GTK_CONTAINER(mis_transp_vbox) , 6 );
  gtk_container_add( GTK_CONTAINER(mis_transp_frame) , mis_transp_vbox );
  gtk_box_pack_start( GTK_BOX(mis_vbox) , mis_transp_frame , false , false , 0 );

  mis_transp_fake_rbt = gtk_radio_button_new_with_label( nullptr ,
                          _("Fake transparency") );
  mis_transp_real_rbt = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(mis_transp_fake_rbt) ,
                          _("Real transparency (requires X Composite Ext.)") );
  g_object_set_data( G_OBJECT(mis_transp_fake_rbt) , "val" ,
                     GINT_TO_POINTER(AOSD_MISC_TRANSPARENCY_FAKE) );
  g_object_set_data( G_OBJECT(mis_transp_real_rbt) , "val" ,
                     GINT_TO_POINTER(AOSD_MISC_TRANSPARENCY_REAL) );
  gtk_box_pack_start( GTK_BOX(mis_transp_vbox) , mis_transp_fake_rbt , true , true , 0 );
  gtk_box_pack_start( GTK_BOX(mis_transp_vbox) , mis_transp_real_rbt , true , true , 0 );

  mis_transp_status_hbox = gtk_hbox_new( false , 4 );
  mis_transp_status_frame = gtk_frame_new( nullptr );
  gtk_container_set_border_width( GTK_CONTAINER(mis_transp_status_hbox) , 3 );
  gtk_container_add( GTK_CONTAINER(mis_transp_status_frame) , mis_transp_status_hbox );
  gtk_box_pack_start( GTK_BOX(mis_transp_vbox) , mis_transp_status_frame , true , true , 0 );

  mis_transp_status_img = gtk_image_new();
  gtk_misc_set_alignment( GTK_MISC(mis_transp_status_img) , 0.5 , 0 );
  mis_transp_status_label = gtk_label_new( "" );
  gtk_misc_set_alignment( GTK_MISC(mis_transp_status_label) , 0 , 0.5 );
  gtk_label_set_line_wrap( GTK_LABEL(mis_transp_status_label) , true );
  gtk_box_pack_start( GTK_BOX(mis_transp_status_hbox) , mis_transp_status_img , false , false , 0 );
  gtk_box_pack_start( GTK_BOX(mis_transp_status_hbox) , mis_transp_status_label , true , true , 0 );
  g_object_set_data( G_OBJECT(mis_transp_status_hbox) , "img" , mis_transp_status_img );
  g_object_set_data( G_OBJECT(mis_transp_status_hbox) , "label" , mis_transp_status_label );

  g_signal_connect( G_OBJECT(mis_transp_real_rbt) , "toggled" ,
    G_CALLBACK(aosd_cb_configure_misc_transp_real_clicked) , mis_transp_status_hbox );

  /* check if the composite extension is loaded */
  if ( aosd_osd_check_composite_ext() )
  {
    if ( cfg->misc.transparency_mode == AOSD_MISC_TRANSPARENCY_FAKE )
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(mis_transp_fake_rbt) , true );
    else
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(mis_transp_real_rbt) , true );
  }
  else
  {
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(mis_transp_fake_rbt) , true );
    gtk_widget_set_sensitive( GTK_WIDGET(mis_transp_real_rbt) , false );
    gtk_image_set_from_icon_name( GTK_IMAGE(mis_transp_status_img) ,
      "dialog-error" , GTK_ICON_SIZE_MENU );
    gtk_label_set_text( GTK_LABEL(mis_transp_status_label) , _("Composite extension not loaded") );
    gtk_widget_set_sensitive( GTK_WIDGET(mis_transp_status_hbox) , false );
  }

  aosd_cb_list.append( mis_transp_vbox , aosd_cb_configure_misc_transp_commit );

  return mis_vbox;
}


static void
aosd_cb_configure_test ( void )
{
  aosd_cfg_t cfg = aosd_cfg_t ();
  aosd_callback_list_run (& cfg);

  char * markup_message = g_markup_printf_escaped
   (_("<span font_desc='%s'>Audacious OSD</span>"),
   (const char *) cfg.text.fonts_name[0]);

  aosd_osd_shutdown (); /* stop any displayed osd */
  aosd_osd_cleanup (); /* just in case it's active */
  aosd_osd_init (cfg.misc.transparency_mode);
  aosd_osd_display (markup_message, & cfg, true);

  g_free (markup_message);
}


static void
aosd_cb_configure_cancel ( void )
{
  aosd_cb_list.clear ();

  aosd_osd_shutdown (); /* stop any displayed osd */
  aosd_osd_cleanup (); /* just in case it's active */
  aosd_osd_init (global_config.misc.transparency_mode);
}


static void
aosd_cb_configure_ok ( void )
{
  aosd_cfg_t cfg = aosd_cfg_t ();

  aosd_callback_list_run (& cfg);
  aosd_cb_list.clear ();

  aosd_osd_shutdown (); /* stop any displayed osd */
  aosd_osd_cleanup (); /* just in case it's active */

  aosd_trigger_stop (global_config.trigger); /* stop triggers */
  global_config = cfg; /* put the new config */
  aosd_cfg_save (cfg); /* save the new configuration on config file */
  aosd_osd_init (cfg.misc.transparency_mode); /* restart osd */
  aosd_trigger_start (cfg.trigger); /* restart triggers */
}


static void *
aosd_ui_configure ( void )
{
  GtkWidget *cfg_nb;
  GtkWidget *cfg_position_widget;
  GtkWidget *cfg_animation_widget;
  GtkWidget *cfg_text_widget;
  GtkWidget *cfg_decoration_widget;
  GtkWidget *cfg_trigger_widget;

  /* create a new configuration object */
  aosd_cfg_t cfg = aosd_cfg_t();
  /* fill it with information from config file */
  aosd_cfg_load( cfg );

  cfg_nb = gtk_notebook_new();
  gtk_notebook_set_tab_pos( GTK_NOTEBOOK(cfg_nb) , GTK_POS_TOP );

  /* add POSITION page */
  cfg_position_widget = aosd_ui_configure_position( &cfg );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_position_widget , gtk_label_new( _("Position") ) );

  /* add ANIMATION page */
  cfg_animation_widget = aosd_ui_configure_animation( &cfg );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_animation_widget , gtk_label_new( _("Animation") ) );

  /* add TEXT page */
  cfg_text_widget = aosd_ui_configure_text( &cfg );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_text_widget , gtk_label_new( _("Text") ) );

  /* add DECORATION page */
  cfg_decoration_widget = aosd_ui_configure_decoration( &cfg );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_decoration_widget , gtk_label_new( _("Decoration") ) );

  /* add TRIGGER page */
  cfg_trigger_widget = aosd_ui_configure_trigger( &cfg );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_trigger_widget , gtk_label_new( _("Trigger") ) );

  /* add MISC page */
  cfg_trigger_widget = aosd_ui_configure_misc( &cfg );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_trigger_widget , gtk_label_new( _("Misc") ) );

  return cfg_nb;
}


const PreferencesWidget AOSD::widgets[] = {
    WidgetCustomGTK (aosd_ui_configure),
    WidgetSeparator ({true}),
    WidgetButton (N_("Test"), {aosd_cb_configure_test, "media-playback-start"})
};

const PluginPreferences AOSD::prefs = {
    {widgets},
    nullptr,  // init
    aosd_cb_configure_ok,
    aosd_cb_configure_cancel
};
