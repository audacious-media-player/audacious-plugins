/*
 * kanashi: iterated javascript-driven visualization framework
 * Copyright (c) 2006, 2007 William Pitcock <nenolod@dereferenced.org>
 * Portions copyright (c) 2001 Jamie Gennis <jgennis@mindspring.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* FIXME: prevent the user from dragging something above the root
   actuator */

#include <config.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <audacious/plugin.h>

#include <math.h>

#include "kanashi.h"

/* DON'T CALL kanashi_fatal_error () IN HERE!!! */

/* Actuator page stuffs */
static GtkWidget *cfg_dialog;

/* If selector != NULL, then it's 'OK', otherwise it's 'Cancel' */
static void
load_sel_cb (GtkButton *button, GtkFileSelection *selector)
{
  if (selector)
    {
      static const char *fname;
      mcs_handle_t *db;

      db = aud_cfg_db_open();
      fname = (char *) gtk_file_selection_get_filename (selector);
      kanashi_load_preset (fname);
      aud_cfg_db_set_string(db, "kanashi", "last_path", (char*)fname);
      aud_cfg_db_close(db);
    }

  gtk_widget_set_sensitive (cfg_dialog, TRUE);
}

static void
load_button_cb (GtkButton *button, gpointer data)
{
  GtkWidget *selector;
  mcs_handle_t *db;
  gchar *last_path;

  db = aud_cfg_db_open();
  selector = gtk_file_selection_new ("Load Preset");
  if(aud_cfg_db_get_string(db, "kanashi", "last_path", &last_path)) {
     gtk_file_selection_set_filename(GTK_FILE_SELECTION(selector), last_path);
  }
  aud_cfg_db_close(db);

  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (selector)->ok_button),
		      "clicked", GTK_SIGNAL_FUNC (load_sel_cb), selector);
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (selector)->cancel_button),
		      "clicked", GTK_SIGNAL_FUNC (load_sel_cb), NULL);

  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (selector)->ok_button),
			     "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     (gpointer) selector);
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (selector)->cancel_button),
			     "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     (gpointer) selector);

  gtk_widget_set_sensitive (cfg_dialog, FALSE);
  gtk_widget_show (selector);
}

static void
ok_button_cb (GtkButton *button, gpointer data)
{
  gtk_widget_hide (cfg_dialog);
}

void
kanashi_configure (void)
{
  GtkWidget *bbox, *button;

  if (! cfg_dialog)
    {
      /* The dialog */
      cfg_dialog = gtk_dialog_new ();
      gtk_window_set_title (GTK_WINDOW (cfg_dialog), "Kanashi");
      gtk_widget_set_usize (cfg_dialog, 530, 370);
      gtk_container_border_width (GTK_CONTAINER (cfg_dialog), 8);
      gtk_signal_connect_object (GTK_OBJECT (cfg_dialog), "delete-event",
				 GTK_SIGNAL_FUNC (gtk_widget_hide),
				 GTK_OBJECT (cfg_dialog));

      /* OK / Apply / Cancel */
      bbox = gtk_hbutton_box_new ();
      gtk_widget_show (bbox);
      gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
      gtk_button_box_set_spacing (GTK_BUTTON_BOX (bbox), 8);
      gtk_button_box_set_child_size (GTK_BUTTON_BOX (bbox), 64, 0);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (cfg_dialog)->action_area),
			  bbox, FALSE, FALSE, 0);

      button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
      gtk_widget_show (button);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (load_button_cb), NULL);
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NORMAL);
      gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

      button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
      gtk_widget_show (button);
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NORMAL);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (ok_button_cb), NULL);
      gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
    }

  gtk_widget_show (cfg_dialog);
  gtk_widget_grab_focus (cfg_dialog);
}
