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

#include <stdlib.h>
#include "iris.h"

/* declaration of all the themes start here */
extern iris_theme theme_original;
extern iris_theme theme_spectrum;
extern iris_theme theme_spectrotoy;
extern iris_theme theme_squarefield;
extern iris_theme theme_waves;
extern iris_theme theme_pyramid;
extern iris_theme theme_knot;
extern iris_theme theme_pinwheel;
extern iris_theme theme_pipes;
extern iris_theme theme_float;
extern iris_theme theme_fountain;
extern iris_theme theme_flash;

/* this is the array where all the themes are */
iris_theme theme[THEME_NUMBER];


/* this initialize the theme registry */
void
theme_register (void)
{
  theme[0] = theme_original;
  theme[1] = theme_spectrum;
  theme[2] = theme_spectrotoy;
  theme[3] = theme_squarefield;
  theme[4] = theme_waves;
  theme[5] = theme_pyramid;
  theme[6] = theme_knot;
  theme[7] = theme_pinwheel;
  theme[8] = theme_pipes;
  theme[9] = theme_float;
  theme[10] = theme_fountain;
  theme[11] = theme_flash;}


/* allocate memory for the config struct of the themes */
void
theme_config_init (void)
{
  int i;

  for (i = 0; i < THEME_NUMBER; i++)
    {
      theme[i].config->global = g_malloc (sizeof (config_global));
      theme[i].config_new->global = g_malloc (sizeof (config_global));
    }
}


/* set the default global config of a theme */
void
theme_config_global_default (int num)
{
  theme[num].config->global->priority = 1.0;	/* priority set to max */
  theme[num].config->global->transparency = -1;	/* transparency random */
  theme[num].config->global->wireframe = 0;	/* wireframe off */
}


/* read the global config of a theme
   (priority, transparency, wireframe) */
void
theme_config_global_read (ConfigDb * db, char *section_name, int num)
{
  char *string;

  string = g_strconcat (theme[num].key, "_", "priority", NULL);
  bmp_cfg_db_get_float (db, section_name, string, &theme[num].config->global->priority);
  g_free (string);

  string = g_strconcat (theme[num].key, "_", "transparency", NULL);
  bmp_cfg_db_get_int (db, section_name, string, &theme[num].config->global->transparency);
  g_free (string);

  string = g_strconcat (theme[num].key, "_", "wireframe", NULL);
  bmp_cfg_db_get_int (db, section_name, string, &theme[num].config->global->wireframe);
  g_free (string);
}


/* write the global config of a theme */
void
theme_config_global_write (ConfigDb * db, char *section_name, int num)
{
  char *string;

  string = g_strconcat (theme[num].key, "_", "priority", NULL);
  bmp_cfg_db_set_float (db, section_name, string, theme[num].config->global->priority);
  g_free (string);

  string = g_strconcat (theme[num].key, "_", "transparency", NULL);
  bmp_cfg_db_set_int (db, section_name, string, theme[num].config->global->transparency);
  g_free (string);

  string = g_strconcat (theme[num].key, "_", "wireframe", NULL);
  bmp_cfg_db_set_int (db, section_name, string, theme[num].config->global->wireframe);
  g_free (string);
}


/* the following 6 functions are callbacks for the theme_config_global_widgets function */
void
on_rb_transparency_random (GtkWidget * widget, gpointer data)
{
  theme[(int) data].config_new->global->transparency = -1;
}


void
on_rb_transparency_on (GtkWidget * widget, gpointer data)
{
  theme[(int) data].config_new->global->transparency = 1;
}


void
on_rb_transparency_off (GtkWidget * widget, gpointer data)
{
  theme[(int) data].config_new->global->transparency = 0;
}


void
on_rb_wireframe_random (GtkWidget * widget, gpointer data)
{
  theme[(int) data].config_new->global->wireframe = -1;
}


void
on_rb_wireframe_on (GtkWidget * widget, gpointer data)
{
  theme[(int) data].config_new->global->wireframe = 1;
}


void
on_rb_wireframe_off (GtkWidget * widget, gpointer data)
{
  theme[(int) data].config_new->global->wireframe = 0;
}


void
theme_config_apply (int num)
{
  memcpy (theme[num].config->private, theme[num].config_new->private,
	  theme[num].config_private_size);
}


/* create the config widgets of all the themes */
void
theme_config_global_widgets (GtkWidget * vbox, int num)
{
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *radio_button_on1;
  GtkWidget *radio_button_off1;
  GtkWidget *radio_button_random1;
  GSList *group;

  memcpy (theme[num].config_new->global, theme[num].config->global,
	  sizeof (config_global));

  /* transparency mode */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Transparency");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
  radio_button_random1 = gtk_radio_button_new_with_label (NULL, "Random");
  gtk_box_pack_start (GTK_BOX (hbox), radio_button_random1, FALSE, FALSE, 4);

  group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio_button_random1));
  radio_button_on1 = gtk_radio_button_new_with_label (group, "On");
  gtk_box_pack_start (GTK_BOX (hbox), radio_button_on1, FALSE, FALSE, 4);

  group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio_button_on1));
  radio_button_off1 = gtk_radio_button_new_with_label (group, "Off");
  gtk_box_pack_start (GTK_BOX (hbox), radio_button_off1, FALSE, FALSE, 4);

  switch (theme[num].config->global->transparency)
    {
    case -1:
      {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				      (radio_button_random1), TRUE);
	break;
      }
    case 0:
      {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				      (radio_button_off1), TRUE);
	break;
      }
    case 1:
      {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				      (radio_button_on1), TRUE);
      }
    }

  gtk_signal_connect (GTK_OBJECT (radio_button_random1), "toggled",
		      GTK_SIGNAL_FUNC (on_rb_transparency_random),
		      (gpointer) num);
  gtk_signal_connect (GTK_OBJECT (radio_button_off1), "toggled",
		      GTK_SIGNAL_FUNC (on_rb_transparency_off),
		      (gpointer) num);
  gtk_signal_connect (GTK_OBJECT (radio_button_on1), "toggled",
		      GTK_SIGNAL_FUNC (on_rb_transparency_on),
		      (gpointer) num);

  /* wireframe mode */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Wireframe");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
  radio_button_random1 = gtk_radio_button_new_with_label (NULL, "Random");
  gtk_box_pack_start (GTK_BOX (hbox), radio_button_random1, FALSE, FALSE, 4);

  group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio_button_random1));
  radio_button_on1 = gtk_radio_button_new_with_label (group, "On");
  gtk_box_pack_start (GTK_BOX (hbox), radio_button_on1, FALSE, FALSE, 4);

  group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio_button_on1));
  radio_button_off1 = gtk_radio_button_new_with_label (group, "Off");
  gtk_box_pack_start (GTK_BOX (hbox), radio_button_off1, FALSE, FALSE, 4);

  switch (theme[num].config->global->wireframe)
    {
    case -1:
      {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				      (radio_button_random1), TRUE);
	break;
      }
    case 0:
      {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				      (radio_button_off1), TRUE);
	break;
      }
    case 1:
      {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				      (radio_button_on1), TRUE);
      }
    }

  gtk_signal_connect (GTK_OBJECT (radio_button_random1), "toggled",
		      GTK_SIGNAL_FUNC (on_rb_wireframe_random),
		      (gpointer) num);
  gtk_signal_connect (GTK_OBJECT (radio_button_off1), "toggled",
		      GTK_SIGNAL_FUNC (on_rb_wireframe_off), (gpointer) num);
  gtk_signal_connect (GTK_OBJECT (radio_button_on1), "toggled",
		      GTK_SIGNAL_FUNC (on_rb_wireframe_on), (gpointer) num);

}


/* create the theme about vbox */
void
theme_about (GtkWidget * vbox_about, int num)
{
  GtkWidget *label;
  GtkWidget *vbox;

  vbox = gtk_vbox_new (FALSE, 4);
  label = gtk_label_new (g_strconcat ("Theme name: ", theme[num].name, NULL));
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);

  label =
    gtk_label_new (g_strconcat ("Theme author: ", theme[num].author, NULL));
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);

  label =
    gtk_label_new (g_strconcat
		   ("Theme description: ", theme[num].description, NULL));
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);

  gtk_box_pack_start (GTK_BOX (vbox_about), vbox, FALSE, FALSE, 4);
}
