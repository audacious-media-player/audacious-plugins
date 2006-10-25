/*  Iris - visualization plugin for XMMS
 *  Copyright (C) 2000-2002 Cédric DELFOSSE (cdelfosse@free.fr)
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <audacious/configdb.h>
#include <GL/gl.h>
#include "iris.h"
#include "config.h"

iris_config config;
iris_config newconfig;
GLWindow GLWin;

static char section_name[] = "iris";

GtkWidget *config_window;

GtkWidget *preview_color_one_color;
GtkWidget *preview_color_two_colors_1;
GtkWidget *preview_color_two_colors_2;
GtkWidget *preview_background;
GtkWidget *preview_flash;

static GtkWidget *config_ctree;
static GtkWidget *config_notebook;
static gint config_page;

static gboolean
check_cfg_version (ConfigDb * db)
{
  char *vstr;

  if (bmp_cfg_db_get_string (db, "iris", "version", &vstr))
    if (!strcmp (vstr, VERSION))
      return FALSE;
  return TRUE;
}


void
iris_set_default_prefs (void)
{
  int i;

  for (i = 0; i < THEME_NUMBER; i++)
    {
      theme_config_global_default (i);
      if (theme[i].config_default != NULL)
	theme[i].config_default ();
    }

  config.bgc_red = 0.0;
  config.bgc_green = 0.0;
  config.bgc_blue = 0.0;

  config.bgc_random = FALSE;

  config.color_red = 0.0;
  config.color_green = 0.0;
  config.color_blue = 1.0;

  config.color_random = TRUE;

  config.color1_red = 1.0;
  config.color1_green = 0.0;
  config.color1_blue = 0.0;

  config.color2_red = 0.0;
  config.color2_green = 1.0;
  config.color2_blue = 0.0;

  config.color12_random = TRUE;

  config.color_flash_red = 1.0;
  config.color_flash_green = 1.0;
  config.color_flash_blue = 1.0;

  config.color_mode = 1;

  config.color_beat = TRUE;

  config.flash_speed = 5;

  config.fps = 50;

  config.change_theme_on_beat = FALSE;

  config.fs_width = 640;
  config.fs_height = 480;

  config.window_width = 640;
  config.window_height = 480;

  config.fullscreen = FALSE;

  config.transition = TRUE;
  config.trans_duration = 10.0;
}


void
iris_config_read (void)
{
  ConfigDb *db;

  db = bmp_cfg_db_open();
  
  /* Check the config version string. If the string is too old, default values
   * are installed. If the string returns NULL, iris has not been run before,
   * and default values are installed. */
  if (check_cfg_version (db))
  {
    printf("Bad iris plugin version detected in config, using default configuration\n");
    iris_set_default_prefs ();
  }
  else
  {
    int i;

    /* load config of all the theme */
    for (i = 0; i < THEME_NUMBER; i++)
    {
      theme_config_global_read (db, section_name, i);
      if (theme[i].config_read != NULL)
      theme[i].config_read (db, section_name);
    }

    bmp_cfg_db_get_float  (db, section_name, "bgc_red", &config.bgc_red);
    bmp_cfg_db_get_float  (db, section_name, "bgc_green", &config.bgc_green);
    bmp_cfg_db_get_float  (db, section_name, "bgc_blue", &config.bgc_blue);
    bmp_cfg_db_get_bool   (db, section_name, "bgc_random", &config.bgc_random);
    bmp_cfg_db_get_float  (db, section_name, "color_red", &config.color_red);
    bmp_cfg_db_get_float  (db, section_name, "color_green", &config.color_green);
    bmp_cfg_db_get_float  (db, section_name, "color_blue", &config.color_blue);
    bmp_cfg_db_get_bool	  (db, section_name, "color_random", &config.color_random);
    bmp_cfg_db_get_float  (db, section_name, "color1_red", &config.color1_red);
    bmp_cfg_db_get_float  (db, section_name, "color1_green", &config.color1_green);
    bmp_cfg_db_get_float  (db, section_name, "color1_blue", &config.color1_blue);
    bmp_cfg_db_get_float  (db, section_name, "color2_red", &config.color2_red);
    bmp_cfg_db_get_float  (db, section_name, "color2_green", &config.color2_green);
    bmp_cfg_db_get_float  (db, section_name, "color2_blue", &config.color2_blue);
    bmp_cfg_db_get_bool	  (db, section_name, "color12_random", &config.color12_random);
    bmp_cfg_db_get_float  (db, section_name, "color_flash_red", &config.color_flash_red);
    bmp_cfg_db_get_float  (db, section_name, "color_flash_green", &config.color_flash_green);
    bmp_cfg_db_get_float  (db, section_name, "color_flash_blue", &config.color_flash_blue);
    bmp_cfg_db_get_int	  (db, section_name, "color_mode", &config.color_mode);
    bmp_cfg_db_get_bool	  (db, section_name, "color_beat", &config.color_beat);
    bmp_cfg_db_get_int	  (db, section_name, "flash_speed", &config.flash_speed);
    bmp_cfg_db_get_int	  (db, section_name, "fps", &config.fps);
    bmp_cfg_db_get_bool	  (db, section_name, "change_theme_on_beat", &config.change_theme_on_beat);
    bmp_cfg_db_get_int	  (db, section_name, "fs_width", &config.fs_width);
    bmp_cfg_db_get_int	  (db, section_name, "fs_height", &config.fs_height);
    bmp_cfg_db_get_int	  (db, section_name, "window_width", &config.window_width);
    bmp_cfg_db_get_int	  (db, section_name, "window_height", &config.window_height);
    bmp_cfg_db_get_bool	  (db, section_name, "fullscreen", &config.fullscreen);
    bmp_cfg_db_get_bool	  (db, section_name, "transition", &config.transition);
    bmp_cfg_db_get_float  (db, section_name, "trans_duration", &config.trans_duration);
  }
  bmp_cfg_db_close(db);
}


void
iris_config_write (iris_config * config)
{
  ConfigDb *db;
  int i;

  db = bmp_cfg_db_open();
  
  /* save config of all the theme */
  for (i = 0; i < THEME_NUMBER; i++)
    {
      theme_config_global_write (db, section_name, i);
      if (theme[i].config_write != NULL)
	theme[i].config_write (db, section_name);
    }

  /* Save the current configuration in the "iris" section */
  bmp_cfg_db_set_string	(db, section_name, "version", VERSION);
  bmp_cfg_db_set_float	(db, section_name, "bgc_red", config->bgc_red);
  bmp_cfg_db_set_float	(db, section_name, "bgc_green", config->bgc_green);
  bmp_cfg_db_set_float	(db, section_name, "bgc_blue", config->bgc_blue);
  bmp_cfg_db_set_bool	(db, section_name, "bgc_random", config->bgc_random);
  bmp_cfg_db_set_float	(db, section_name, "color_red", config->color_red);
  bmp_cfg_db_set_float	(db, section_name, "color_green", config->color_green);
  bmp_cfg_db_set_float	(db, section_name, "color_blue", config->color_blue);
  bmp_cfg_db_set_bool	(db, section_name, "color_random", config->color_random);
  bmp_cfg_db_set_float	(db, section_name, "color1_red", config->color1_red);
  bmp_cfg_db_set_float	(db, section_name, "color1_green", config->color1_green);
  bmp_cfg_db_set_float	(db, section_name, "color1_blue", config->color1_blue);
  bmp_cfg_db_set_float	(db, section_name, "color2_red", config->color2_red);
  bmp_cfg_db_set_float	(db, section_name, "color2_green", config->color2_green);
  bmp_cfg_db_set_float	(db, section_name, "color2_blue", config->color2_blue);
  bmp_cfg_db_set_bool	(db, section_name, "color12_random", config->color12_random);
  bmp_cfg_db_set_float	(db, section_name, "color_flash_red", config->color_flash_red);
  bmp_cfg_db_set_float	(db, section_name, "color_flash_green", config->color_flash_green);
  bmp_cfg_db_set_float	(db, section_name, "color_flash_blue", config->color_flash_blue);
  bmp_cfg_db_set_int	(db, section_name, "color_mode", config->color_mode);
  bmp_cfg_db_set_bool	(db, section_name, "color_beat", config->color_beat);
  bmp_cfg_db_set_int	(db, section_name, "flash_speed", config->flash_speed);
  bmp_cfg_db_set_int	(db, section_name, "fps", config->fps);
  bmp_cfg_db_set_bool	(db, section_name, "change_theme_on_beat", config->change_theme_on_beat);
  bmp_cfg_db_set_int	(db, section_name, "fs_width", config->fs_width);
  bmp_cfg_db_set_int	(db, section_name, "fs_height", config->fs_height);
  bmp_cfg_db_set_int	(db, section_name, "window_width", config->window_width);
  bmp_cfg_db_set_int	(db, section_name, "window_height", config->window_height);
  bmp_cfg_db_set_bool	(db, section_name, "fullscreen", config->fullscreen);
  bmp_cfg_db_set_bool	(db, section_name, "transition", config->transition);
  bmp_cfg_db_set_float	(db, section_name, "trans_duration", config->trans_duration);

  /* write out the XMMS config file */
  bmp_cfg_db_close(db);
}


static void
set_color_preview (GLfloat red, GLfloat green, GLfloat blue,
		   GtkWidget * preview)
{
  unsigned int a;
  guchar buf[3 * 32];
  char r, g, b;
  char s[3];

  // how to convert a float into a char ?
  sprintf (s, "%.0f", red * 255);
  r = (unsigned char) atoi (s);
  sprintf (s, "%.0f", green * 255);
  g = (unsigned char) atoi (s);
  sprintf (s, "%.0f", blue * 255);
  b = (unsigned char) atoi (s);

  for (a = 0; a < 32; a++)
    {
      buf[3 * a] = (char) r;
      buf[3 * a + 1] = (char) g;
      buf[3 * a + 2] = (char) b;
    }

  for (a = 0; a < 16; a++)
    gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, a, 32);

  gtk_widget_draw (preview, NULL);
}


static void
csel_ok (GtkWidget * w, GtkWidget * csel)
{
  gdouble a[3];

  gtk_color_selection_get_color (GTK_COLOR_SELECTION
				 (GTK_COLOR_SELECTION_DIALOG (csel)->
				  colorsel), &a[0]);

  gtk_widget_destroy (csel);
}


static void
csel_ok2 (GtkWidget * csel, gpointer data)
{
  gdouble a[3];

  gtk_color_selection_get_color (GTK_COLOR_SELECTION
				 (GTK_COLOR_SELECTION_DIALOG (csel)->
				  colorsel), &a[0]);

  switch ((int) data)
    {
    case 0:
      {
	newconfig.color_red = (GLfloat) a[0];
	newconfig.color_green = (GLfloat) a[1];
	newconfig.color_blue = (GLfloat) a[2];
	set_color_preview (newconfig.color_red, newconfig.color_green,
			   newconfig.color_blue, preview_color_one_color);
	break;
      }
    case 1:
      {
	newconfig.color1_red = (GLfloat) a[0];
	newconfig.color1_green = (GLfloat) a[1];
	newconfig.color1_blue = (GLfloat) a[2];
	set_color_preview (newconfig.color1_red, newconfig.color1_green,
			   newconfig.color1_blue, preview_color_two_colors_1);
	break;
      }
    case 2:
      {
	newconfig.color2_red = (GLfloat) a[0];
	newconfig.color2_green = (GLfloat) a[1];
	newconfig.color2_blue = (GLfloat) a[2];
	set_color_preview (newconfig.color2_red, newconfig.color2_green,
			   newconfig.color2_blue, preview_color_two_colors_2);
	break;
      }
    case 3:
      {
	newconfig.bgc_red = (GLfloat) a[0];
	newconfig.bgc_green = (GLfloat) a[1];
	newconfig.bgc_blue = (GLfloat) a[2];
	set_color_preview (newconfig.bgc_red, newconfig.bgc_green,
			   newconfig.bgc_blue, preview_background);
	break;
      }
    case 4:
      {
	newconfig.color_flash_red = (GLfloat) a[0];
	newconfig.color_flash_green = (GLfloat) a[1];
	newconfig.color_flash_blue = (GLfloat) a[2];
	set_color_preview (newconfig.color_flash_red,
			   newconfig.color_flash_green,
			   newconfig.color_flash_blue, preview_flash);
	break;
      }
    }

  gtk_widget_destroy (csel);
}


static void
apply_clicked (void)
{
  gint i;

  memcpy (&config, &newconfig, sizeof (iris_config));
  for (i = 0; i < THEME_NUMBER; i++)
    {
      memcpy (theme[i].config->global, theme[i].config_new->global,
	      sizeof (config_global));
      theme_config_apply (i);
    }
}


static void
cancel_clicked (GtkWidget * w, GtkWidget * window)
{
  gtk_widget_destroy (window);
  config_window = NULL;
}


static void
csel_deleteevent (GtkWidget * w, GtkWidget * csel)
{
  gtk_widget_destroy (csel);
}


static void
ok_clicked (GtkWidget * w, GtkWidget * window)
{
  apply_clicked ();
  iris_config_write (&newconfig);
  cancel_clicked (w, window);
}


static void
color_clicked (GtkWidget * w, gpointer data)
{
  GtkWidget *csel;
  gdouble a[3];

  switch ((int) data)
    {
    case 0:
      {
	a[0] = (gdouble) newconfig.color_red;
	a[1] = (gdouble) newconfig.color_green;
	a[2] = (gdouble) newconfig.color_blue;
	break;
      }
    case 1:
      {
	a[0] = (gdouble) newconfig.color1_red;
	a[1] = (gdouble) newconfig.color1_green;
	a[2] = (gdouble) newconfig.color1_blue;
	break;
      }
    case 2:
      {
	a[0] = (gdouble) newconfig.color2_red;
	a[1] = (gdouble) newconfig.color2_green;
	a[2] = (gdouble) newconfig.color2_blue;
	break;
      }
    case 3:
      {
	a[0] = (gdouble) newconfig.bgc_red;
	a[1] = (gdouble) newconfig.bgc_green;
	a[2] = (gdouble) newconfig.bgc_blue;
	break;
      }
    case 4:
      {
	a[0] = (gdouble) newconfig.color_flash_red;
	a[1] = (gdouble) newconfig.color_flash_green;
	a[2] = (gdouble) newconfig.color_flash_blue;
	break;
      }
    }

  csel = gtk_color_selection_dialog_new ("Please choose a color");
  /*gtk_window_set_modal (GTK_WINDOW
			(&(GTK_COLOR_SELECTION_DIALOG (csel)->window)), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW
				(&
				 (GTK_COLOR_SELECTION_DIALOG (csel)->window)),
				GTK_WINDOW (config_window)); */
  gtk_widget_hide (GTK_COLOR_SELECTION_DIALOG (csel)->help_button);
  gtk_widget_hide (GTK_COLOR_SELECTION_DIALOG (csel)->cancel_button);
  gtk_color_selection_set_color (GTK_COLOR_SELECTION
				 (GTK_COLOR_SELECTION_DIALOG (csel)->
				  colorsel), &a[0]);
  gtk_widget_show (csel);

  gtk_signal_connect (GTK_OBJECT
		      (GTK_COLOR_SELECTION_DIALOG (csel)->ok_button),
		      "clicked", GTK_SIGNAL_FUNC (csel_ok), csel);
  gtk_signal_connect (GTK_OBJECT (csel), "delete_event",
		      GTK_SIGNAL_FUNC (csel_deleteevent), csel);
  gtk_signal_connect (GTK_OBJECT (csel), "destroy",
		      GTK_SIGNAL_FUNC (csel_ok2), data);
}


void
colormode_toggled (GtkWidget * widget, gpointer data)
{
  newconfig.color_mode = (unsigned int) data;
}


void
colorbeat_toggled (GtkWidget * widget, gpointer data)
{
  newconfig.color_beat = !newconfig.color_beat;
}


void
theme_beat_toggled (GtkWidget * widget, gpointer data)
{
  newconfig.change_theme_on_beat = !newconfig.change_theme_on_beat;
}


void
color_random_toggled (GtkWidget * widget, gpointer data)
{
  newconfig.color_random = !newconfig.color_random;
}


void
color12_random_toggled (GtkWidget * widget, gpointer data)
{
  newconfig.color12_random = !newconfig.color12_random;
}


void
bgc_random_toggled (GtkWidget * widget, gpointer data)
{
  newconfig.bgc_random = !newconfig.bgc_random;
}


void
wireframe_toggled (GtkWidget * widget, gpointer * data)
{
  newconfig.wireframe = !newconfig.wireframe;
}


void
fullscreen_toggled (GtkWidget * widget, gpointer data)
{
  newconfig.fullscreen = !newconfig.fullscreen;
}

void
transition_toggled (GtkWidget * widget, gpointer data)
{
  newconfig.transition = !newconfig.transition;
}

static void
combo_fs_activated (GtkWidget * widget, gpointer data)
{
  sscanf (gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (data)->entry)), "%dx%d",
	  &newconfig.fs_width, &newconfig.fs_height);
}


static void
value_flash_speed (GtkAdjustment * adj)
{
  newconfig.flash_speed = (int) adj->value;
}

static void
value_trans_duration (GtkAdjustment * adj)
{
  newconfig.trans_duration = (int) adj->value;
}

static void
conf_closed (GtkWidget * w, GdkEvent * e, GtkWidget ** window)
{
  gtk_widget_destroy (*window);
  *window = NULL;
}


static void
priority_value_changed (GtkAdjustment * adj, gint i)
{
  theme[i].config_new->global->priority = adj->value / 100.0;
}


static void
create_config_color (GtkWidget * vbox_container)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GSList *group;

  GtkWidget *radio_button_one_color;
  GtkWidget *button_color_one_color;
  //  GtkWidget *preview_color_one_color; must be static

  GtkWidget *radio_button_two_colors;
  GtkWidget *button_color_two_colors_1;
  GtkWidget *button_color_two_colors_2;
  //  GtkWidget *preview_color_two_colors_1; must be static
  //  GtkWidget *preview_color_two_colors_2; must be static

  GtkWidget *check_button;

  GtkWidget *hseparator;

  GtkWidget *label;
  GtkWidget *button_background;
  //  GtkWidget *preview_background; static

  frame = gtk_frame_new ("color");
  gtk_box_pack_start (GTK_BOX (vbox_container), frame, TRUE, TRUE, 0);
  vbox = gtk_vbox_new (TRUE, 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  /* the first hbox: a radio button and a color selection/preview button */

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 8);

  radio_button_one_color =
    gtk_radio_button_new_with_label (NULL, "one color");
  gtk_box_pack_start (GTK_BOX (hbox), radio_button_one_color, FALSE, FALSE,
		      4);
  gtk_widget_set_usize (radio_button_one_color, 100, 20);
  gtk_widget_show (radio_button_one_color);

  button_color_one_color = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button_color_one_color, FALSE, FALSE,
		      4);
  gtk_widget_show (button_color_one_color);
  preview_color_one_color = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_preview_size (GTK_PREVIEW (preview_color_one_color), 32, 16);
  gtk_widget_show (preview_color_one_color);
  gtk_container_add (GTK_CONTAINER (button_color_one_color),
		     preview_color_one_color);
  set_color_preview (newconfig.color_red, newconfig.color_green,
		     newconfig.color_blue, preview_color_one_color);
  gtk_widget_set_usize (button_color_one_color, 50, 20);
  gtk_signal_connect (GTK_OBJECT (button_color_one_color), "clicked",
		      GTK_SIGNAL_FUNC (color_clicked), (gpointer) 0);

  /* the second hbox: a radio button and two color selection/preview buttons */

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 8);

  group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio_button_one_color));
  radio_button_two_colors =
    gtk_radio_button_new_with_label (group, "two colors");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button_two_colors),
				TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), radio_button_two_colors, FALSE, FALSE,
		      4);
  gtk_widget_set_usize (radio_button_two_colors, 100, 20);
  gtk_widget_show (radio_button_two_colors);

  button_color_two_colors_1 = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button_color_two_colors_1, FALSE, FALSE,
		      4);
  gtk_widget_show (button_color_two_colors_1);
  preview_color_two_colors_1 = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_preview_size (GTK_PREVIEW (preview_color_two_colors_1), 32, 16);
  gtk_widget_show (preview_color_two_colors_1);
  gtk_container_add (GTK_CONTAINER (button_color_two_colors_1),
		     preview_color_two_colors_1);
  set_color_preview (newconfig.color1_red, newconfig.color1_green,
		     newconfig.color1_blue, preview_color_two_colors_1);
  gtk_widget_set_usize (button_color_two_colors_1, 50, 20);

  gtk_signal_connect (GTK_OBJECT (button_color_two_colors_1), "clicked",
		      GTK_SIGNAL_FUNC (color_clicked), (gpointer) 1);

  button_color_two_colors_2 = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button_color_two_colors_2, FALSE, FALSE,
		      4);
  gtk_widget_show (button_color_two_colors_2);
  preview_color_two_colors_2 = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_preview_size (GTK_PREVIEW (preview_color_two_colors_2), 32, 16);
  gtk_widget_show (preview_color_two_colors_2);
  gtk_container_add (GTK_CONTAINER (button_color_two_colors_2),
		     preview_color_two_colors_2);
  set_color_preview (newconfig.color2_red, newconfig.color2_green,
		     newconfig.color2_blue, preview_color_two_colors_2);
  gtk_widget_set_usize (button_color_two_colors_2, 50, 20);

  gtk_signal_connect (GTK_OBJECT (button_color_two_colors_2), "clicked",
		      GTK_SIGNAL_FUNC (color_clicked), (gpointer) 2);

  switch (newconfig.color_mode)
    {
    case 0:
      {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				      (radio_button_one_color), TRUE);
	break;
      }
    case 1:
      {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				      (radio_button_two_colors), TRUE);
      }
    }

  gtk_signal_connect (GTK_OBJECT (radio_button_one_color), "toggled",
		      GTK_SIGNAL_FUNC (colormode_toggled), (gpointer) 0);
  gtk_signal_connect (GTK_OBJECT (radio_button_two_colors), "toggled",
		      GTK_SIGNAL_FUNC (colormode_toggled), (gpointer) 1);

  check_button = gtk_check_button_new_with_label ("Random on beat");
  gtk_widget_show (check_button);
  gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
				newconfig.color12_random);
  gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
		      GTK_SIGNAL_FUNC (color12_random_toggled), NULL);

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, FALSE, 4);

  /* the third hbox: a color selection/preview button */

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Background color");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  button_background = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button_background, FALSE, FALSE, 4);
  gtk_widget_show (button_background);
  preview_background = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_preview_size (GTK_PREVIEW (preview_background), 32, 16);
  gtk_widget_show (preview_background);
  gtk_container_add (GTK_CONTAINER (button_background), preview_background);
  set_color_preview (newconfig.bgc_red, newconfig.bgc_green,
		     newconfig.bgc_blue, preview_background);
  gtk_widget_set_usize (button_background, 50, 20);

  gtk_signal_connect (GTK_OBJECT (button_background), "clicked",
		      GTK_SIGNAL_FUNC (color_clicked), (gpointer) 3);

  check_button = gtk_check_button_new_with_label ("Random on beat");
  gtk_widget_show (check_button);
  gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
				newconfig.bgc_random);
  gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
		      GTK_SIGNAL_FUNC (bgc_random_toggled), NULL);
/*
  check_button = gtk_check_button_new_with_label ("Wireframe");
  gtk_widget_show (check_button);
  gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
				newconfig.wireframe);
  gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
		      GTK_SIGNAL_FUNC (wireframe_toggled), NULL);
*/
  /* end */

}


static void
create_config_beat (GtkWidget * vbox_container)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *hbox;

  GtkWidget *button_beat;
  GtkWidget *button_theme_beat;

  GtkWidget *label;
  GtkWidget *button_flash;
  GtkWidget *preview_flash;

  GtkObject *adjustment;
  GtkWidget *hscale;

  frame = gtk_frame_new ("beat");
  gtk_box_pack_start (GTK_BOX (vbox_container), frame, TRUE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  /* the first hbox: two buttons */

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  button_beat = gtk_check_button_new_with_label ("Flash on beat");
  gtk_widget_show (button_beat);
  gtk_box_pack_start (GTK_BOX (hbox), button_beat, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_beat),
				newconfig.color_beat);
  gtk_signal_connect (GTK_OBJECT (button_beat), "toggled",
		      GTK_SIGNAL_FUNC (colorbeat_toggled), NULL);

  button_theme_beat =
    gtk_check_button_new_with_label ("Change theme on beat");
  gtk_widget_show (button_theme_beat);
  gtk_box_pack_start (GTK_BOX (hbox), button_theme_beat, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_theme_beat),
				newconfig.change_theme_on_beat);
  gtk_signal_connect (GTK_OBJECT (button_theme_beat), "toggled",
		      GTK_SIGNAL_FUNC (theme_beat_toggled), NULL);

  /* the second hbox: a label and a color selection/preview button */

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Flash color");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  button_flash = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button_flash, FALSE, FALSE, 4);
  gtk_widget_show (button_flash);
  preview_flash = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_preview_size (GTK_PREVIEW (preview_flash), 32, 16);
  gtk_widget_show (preview_flash);
  gtk_container_add (GTK_CONTAINER (button_flash), preview_flash);
  set_color_preview (newconfig.color_flash_red, newconfig.color_flash_green,
		     newconfig.color_flash_blue, preview_flash);
  gtk_widget_set_usize (button_flash, 50, 20);
  gtk_signal_connect (GTK_OBJECT (button_flash), "clicked",
		      GTK_SIGNAL_FUNC (color_clicked), (gpointer) 4);

  /* the third hbox: a label and a horizontal scale */

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Flash duration (in frames)");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  adjustment = gtk_adjustment_new (newconfig.flash_speed, 1, 50, 1, 5, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_value_pos (GTK_SCALE (hscale), GTK_POS_LEFT);
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (value_flash_speed), NULL);

  label =
    gtk_label_new
    ("Image are drawn at a rate of 50 frames per second.\n So a flash of 50 frames lasts 1 second.");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);
}


static void
create_config_fs (GtkWidget * vbox_container)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *hbox;

  GtkWidget *button_fs;
  GtkWidget *combobox_fs;

  GtkWidget *label;

  char str[10];

  frame = gtk_frame_new ("fullscreen");
  gtk_box_pack_start (GTK_BOX (vbox_container), frame, TRUE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  /* the first hbox: a button */

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  button_fs = gtk_check_button_new_with_label ("Start in fullscreen mode");
  gtk_widget_show (button_fs);
  gtk_box_pack_start (GTK_BOX (hbox), button_fs, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_fs),
				newconfig.fullscreen);
  gtk_signal_connect (GTK_OBJECT (button_fs), "toggled",
		      GTK_SIGNAL_FUNC (fullscreen_toggled), NULL);

  /* the second hbox: a label combobox */

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Fullscreen resolution");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  combobox_fs = gtk_combo_new ();

  gtk_combo_set_popdown_strings (GTK_COMBO (combobox_fs), GLWin.glist);

  gtk_box_pack_start (GTK_BOX (hbox), combobox_fs, FALSE, FALSE, 4);
  gtk_widget_show (combobox_fs);

  gtk_signal_connect (GTK_OBJECT (GTK_COMBO (combobox_fs)->entry), "changed",
		      GTK_SIGNAL_FUNC (combo_fs_activated), combobox_fs);
  gtk_entry_set_editable (GTK_ENTRY (GTK_COMBO (combobox_fs)->entry), FALSE);
  sprintf (str, "%dx%d", newconfig.fs_width, newconfig.fs_height);
  gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (combobox_fs)->entry), str);

  label = gtk_label_new
    ("Use the f key in the GL window\nto switch to fullscreen and back.\n\
Use the esc key to quit the plugin.\n\
\n\
If you use the sawfish window manager,\n\
you may have problem going fullscreen.\n\
In this case, switch to a console and\n\
type \'killall xmms\'.");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);
}

static void
create_config_transition (GtkWidget * vbox_container)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *hbox;

  GtkWidget *button_ts;
  GtkWidget *label;

  GtkObject *adjustment;
  GtkWidget *hscale;

  frame = gtk_frame_new ("transition");
  gtk_box_pack_start (GTK_BOX (vbox_container), frame, TRUE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  /* the first hbox: a button */

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  button_ts =
    gtk_check_button_new_with_label ("Use transitions on theme change");
  gtk_widget_show (button_ts);
  gtk_box_pack_start (GTK_BOX (hbox), button_ts, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_ts),
				newconfig.transition);
  gtk_signal_connect (GTK_OBJECT (button_ts), "toggled",
		      GTK_SIGNAL_FUNC (transition_toggled), NULL);

  label = gtk_label_new ("Chooses a random transition for now");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);

  /* the second hbox: a label and a horizontal scale */

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Transition duration (in miliseconds)");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (newconfig.trans_duration, 1.0, 50.0, 1.0, 1.0, 1.0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_value_pos (GTK_SCALE (hscale), GTK_POS_LEFT);
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (value_trans_duration), NULL);
}

static void
create_config_glx_info (GtkWidget * vbox_container)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *hbox;

  GtkWidget *label;
  
  frame = gtk_frame_new ("GLX informations");
  gtk_box_pack_start (GTK_BOX (vbox_container), frame, TRUE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  if (GLWin.ctx == NULL)
    {
      label = gtk_label_new ("Launch the plugin and reopen the configure window to see\n\
informations about your GL setup.");
      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);
    }
  else
    {
      char *string;
      string = g_strconcat ("GLX version: ", g_strdup_printf ("%d.%d", GLWin.glxMajorVersion, GLWin.glxMinorVersion), NULL);
      label = gtk_label_new (string);
      free(string);
      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);

      if (GLWin.DRI)
	string = g_strconcat ("Use DRI: ", "yes", NULL);
      else
	string = g_strconcat ("Use DRI: ", "no", NULL);	
      label = gtk_label_new (string);
      free(string);
      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);

      if (GLWin.DoubleBuffered)
	string = g_strconcat ("Double buffered rendering: ", "yes", NULL);
      else
	string = g_strconcat ("Double buffered rendering: : ", "no", NULL);	
      label = gtk_label_new (string);
      free(string);
      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);
    }
}

static void
create_config_theme (GtkWidget * vbox_container)
{
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *table;
  GtkWidget *label;
  gint i, y;

  frame = gtk_frame_new ("Themes priorities");
  gtk_box_pack_start (GTK_BOX (vbox_container), frame, TRUE, TRUE, 4);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  table = gtk_table_new (2, THEME_NUMBER, FALSE);
  gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 12);

  for (i = 0, y = 0; i < THEME_NUMBER; i++)
    {
      GtkObject *adjustment;
      GtkWidget *hscale;

      label = gtk_label_new (theme[i].name);
      gtk_table_attach (GTK_TABLE (table), label, 0, 1, y, y + 1, GTK_EXPAND,
			0, 0, 8);
      adjustment =
	gtk_adjustment_new (theme[i].config->global->priority * 100.0, 0.0,
			    100.0, 1.0, 10.0, 0);
      hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
      gtk_scale_set_digits (GTK_SCALE (hscale), 0);
      gtk_scale_set_value_pos (GTK_SCALE (hscale), GTK_POS_LEFT);
      gtk_table_attach (GTK_TABLE (table), hscale, 1, 2, y, y + 1,
			GTK_EXPAND | GTK_FILL, 0, 0, 8);
      gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			  GTK_SIGNAL_FUNC (priority_value_changed),
			  (void *) i);

      y++;
    }

  gtk_widget_show (frame);
}


static void
cb_select_monitor_config (GtkWidget * ctree, GtkCTreeNode * node)
{
  gint row;

  if (!GTK_CLIST (ctree)->selection)
    return;
  row =
    GPOINTER_TO_INT (gtk_ctree_node_get_row_data (GTK_CTREE (ctree), node));
  gtk_notebook_set_page (GTK_NOTEBOOK (config_notebook), row);
}


static GtkWidget *
create_config_page (GtkNotebook * notebook, gchar * text, GtkCTree * ctree,
		    GtkCTreeNode * node_parent, GtkCTreeNode ** node_result)
{
  GtkWidget *vbox;
  GtkCTreeNode *node;
  gchar *title[1] = { text };

  vbox = gtk_vbox_new (FALSE, 0);
  node = gtk_ctree_insert_node (ctree, node_parent, NULL, title, 0,
				NULL, NULL, NULL, NULL, FALSE, FALSE);
  gtk_ctree_node_set_row_data (ctree, node, GINT_TO_POINTER (config_page++));
  gtk_notebook_append_page (notebook, vbox, NULL);
  if (node_result)
    *node_result = node;
  return vbox;
}


void
iris_first_init (void)
{
  static gboolean init;
  int i;

  if (!init)
    {
      theme_register ();
      theme_config_init ();
      /* here we collect information about resolutions supported by the display */
      /* get a connection */
      GLWin.dpy = XOpenDisplay (0);
      GLWin.screen = DefaultScreen (GLWin.dpy);
      XF86VidModeQueryVersion (GLWin.dpy, &GLWin.vidModeMajorVersion,
			       &GLWin.vidModeMinorVersion);
      XF86VidModeGetAllModeLines (GLWin.dpy, GLWin.screen, &GLWin.modeNum,
				  &GLWin.modes);
      /* save desktop-resolution before switching modes */
      GLWin.deskMode = *(GLWin.modes[0]);

      /* fill an array of string for the config combo box */
      for (i = 0; i < GLWin.modeNum; i++)
	GLWin.glist =
	  g_list_append (GLWin.glist,
			 g_strdup_printf ("%dx%d", GLWin.modes[i]->hdisplay,
					  GLWin.modes[i]->vdisplay));

      init = TRUE;
      XCloseDisplay (GLWin.dpy);
      }
}

void
iris_configure (void)
{
  GtkWidget *config_vbox;

  GtkWidget *vbox, *buttonbox, *ok, *apply, *cancel;
  GtkWidget *config_hbox;
  GtkWidget *config_scrolled;

  gchar *title[1] = { "iris config" };
  GtkCTreeNode *node;
  GtkCTreeNode *node_themes;
  gint i;

  if (config_window)
    return;
  config_page = 0;

  iris_first_init ();
  iris_config_read ();
  memcpy (&newconfig, &config, sizeof (iris_config));

  config_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect (GTK_OBJECT (config_window), "delete_event",
		      GTK_SIGNAL_FUNC (conf_closed), &config_window);
  gtk_container_set_border_width (GTK_CONTAINER (config_window), 6);
  gtk_window_set_title (GTK_WINDOW (config_window), "iris configuration");

  config_hbox = gtk_hbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (config_window), config_hbox);

  config_scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (config_scrolled),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (config_hbox), config_scrolled, TRUE, TRUE, 0);

  config_vbox = gtk_vbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (config_hbox), config_vbox, TRUE, TRUE, 0);

  config_ctree = gtk_ctree_new_with_titles (1, 0, title);
  gtk_ctree_set_indent (GTK_CTREE (config_ctree), 16);
  gtk_clist_column_titles_passive (GTK_CLIST (config_ctree));
  gtk_widget_set_usize (config_ctree, 150, 0);
  gtk_container_add (GTK_CONTAINER (config_scrolled), config_ctree);
  gtk_signal_connect (GTK_OBJECT (config_ctree), "tree_select_row",
		      (GtkSignalFunc) cb_select_monitor_config, NULL);
  config_notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (config_notebook), FALSE);
  gtk_box_pack_start (GTK_BOX (config_vbox), config_notebook, TRUE, TRUE, 0);

  vbox = create_config_page (GTK_NOTEBOOK (config_notebook), "Color",
			     GTK_CTREE (config_ctree), NULL, &node);
  create_config_color (vbox);
  gtk_ctree_select (GTK_CTREE (config_ctree), node);

  vbox = create_config_page (GTK_NOTEBOOK (config_notebook), "Beat",
			     GTK_CTREE (config_ctree), NULL, NULL);
  create_config_beat (vbox);

  vbox = create_config_page (GTK_NOTEBOOK (config_notebook),
			     "Transition", GTK_CTREE (config_ctree),
			     NULL, NULL);
  create_config_transition (vbox);

  vbox = create_config_page (GTK_NOTEBOOK (config_notebook),
			     "Fullscreen", GTK_CTREE (config_ctree),
			     NULL, NULL);
  create_config_fs (vbox);

  vbox = create_config_page (GTK_NOTEBOOK (config_notebook), "Themes",
			     GTK_CTREE (config_ctree), NULL, &node_themes);
  create_config_theme (vbox);

  for (i = 0; i < THEME_NUMBER; i++)
    {
      GtkWidget *tabs;
      GtkWidget *vbox_1, *vbox_2, *vbox_3;
      GtkWidget *label;
      vbox =
	create_config_page (GTK_NOTEBOOK (config_notebook),
			    theme[i].name,
			    GTK_CTREE (config_ctree), node_themes, &node);

      tabs = gtk_notebook_new ();
      gtk_box_pack_start (GTK_BOX (vbox), tabs, TRUE, TRUE, 4);

      /* first tab : global conf tab */
      vbox_1 = gtk_vbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (vbox_1), 0);
      theme_config_global_widgets (vbox_1, i);

      label = gtk_label_new ("global settings");
      gtk_notebook_append_page (GTK_NOTEBOOK (tabs), vbox_1, label);

      /* second tab : specific theme tab */
      if (theme[i].config_create != NULL)
	{
	  vbox_2 = gtk_vbox_new (FALSE, 2);
	  theme[i].config_create (vbox_2);

	  label = gtk_label_new ("theme settings");
	  gtk_notebook_append_page (GTK_NOTEBOOK (tabs), vbox_2, label);
	}

      /* third tab : info tab */
      vbox_3 = gtk_vbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (vbox_3), 0);
      theme_about (vbox_3, i);

      label = gtk_label_new ("theme info");
      gtk_notebook_append_page (GTK_NOTEBOOK (tabs), vbox_3, label);

    }

  vbox = create_config_page (GTK_NOTEBOOK (config_notebook),
			     "GLX infos", GTK_CTREE (config_ctree),
			     NULL, NULL);
  create_config_glx_info (vbox);

  //  vbox = create_config_page(GTK_NOTEBOOK(config_notebook), _("About"),
  //                GTK_CTREE(config_ctree), NULL, NULL);
  // create_config_about(vbox);

  /* this is the box where are Ok, Cancel and Apply */
  buttonbox = gtk_hbutton_box_new ();
  gtk_box_pack_end (GTK_BOX (config_vbox), buttonbox, FALSE, FALSE, 8);
  gtk_hbutton_box_set_layout_default (GTK_BUTTONBOX_END);
  gtk_widget_show (buttonbox);

  ok = gtk_button_new_with_label ("Quit\nand save");
  GTK_WIDGET_SET_FLAGS (ok, GTK_CAN_DEFAULT);
  gtk_box_pack_end (GTK_BOX (buttonbox), ok, FALSE, FALSE, 8);
  gtk_widget_show (ok);

  cancel = gtk_button_new_with_label ("Quit\nwithout saving");
  GTK_WIDGET_SET_FLAGS (cancel, GTK_CAN_DEFAULT);
  gtk_box_pack_end (GTK_BOX (buttonbox), cancel, FALSE, FALSE, 8);
  gtk_widget_show (cancel);

  apply = gtk_button_new_with_label ("Apply");
  GTK_WIDGET_SET_FLAGS (apply, GTK_CAN_DEFAULT);
  gtk_box_pack_end (GTK_BOX (buttonbox), apply, FALSE, FALSE, 8);
  gtk_widget_show (apply);

  gtk_window_set_default (GTK_WINDOW (config_window), ok);

  gtk_signal_connect (GTK_OBJECT (cancel), "clicked",
		      GTK_SIGNAL_FUNC (cancel_clicked), config_window);
  gtk_signal_connect (GTK_OBJECT (ok), "clicked",
		      GTK_SIGNAL_FUNC (ok_clicked), config_window);
  gtk_signal_connect (GTK_OBJECT (apply), "clicked",
		      GTK_SIGNAL_FUNC (apply_clicked), config_window);

  gtk_widget_show_all (config_window);
}

/* 
** Saves all window attributes (goal)
** saves only non-fullscreen window sizes for now
** ADDME : window position
*/
void
iris_save_window_attributes (void)
{
  XWindowAttributes attr;
  if (!GLWin.fs)
    {
      XGetWindowAttributes (GLWin.dpy, GLWin.window, &attr);
      config.window_width = attr.width;
      config.window_height = attr.height;
      iris_config_write (&config);
    }
}
