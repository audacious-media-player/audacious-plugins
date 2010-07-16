/*
 * paranormal: iterated pipeline-driven visualization plugin
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
#include <math.h>

#include <audacious/configdb.h>
#include <audacious/plugin.h>

#include "paranormal.h"
#include "actuators.h"
#include "containers.h"
#include "presets.h"

/* DON'T CALL pn_fatal_error () IN HERE!!! */

/* Actuator page stuffs */
static GtkWidget *cfg_dialog, *actuator_tree, *option_frame, *actuator_option_table;
static GtkWidget *actuator_add_opmenu, *actuator_add_button, *actuator_remove_button;
static GtkCTreeNode *selected_actuator_node;
static GtkTooltips *actuator_tooltips;

/* This is used so that actuator_row_data_destroyed_cb won't free
   the actuator associated w/ the node since we're going to be using it */
gboolean destroy_row_data = TRUE;

static void
actuator_row_data_destroyed_cb (struct pn_actuator *a)
{
  if (a && destroy_row_data)
    destroy_actuator (a);
}

static void
add_actuator (struct pn_actuator *a, GtkCTreeNode *parent, gboolean copy)
{
  GtkCTreeNode *node;
  GSList *l;

  g_assert (cfg_dialog);
  g_assert (actuator_tree);
  g_assert (actuator_option_table);

  node = gtk_ctree_insert_node (GTK_CTREE (actuator_tree), parent,
				NULL, (gchar**)&a->desc->dispname, 0,
				NULL, NULL, NULL, NULL,
				a->desc->flags & ACTUATOR_FLAG_CONTAINER
				? FALSE : TRUE,
				TRUE);

  if (a->desc->flags & ACTUATOR_FLAG_CONTAINER)
    for (l=*(GSList **)a->data; l; l = l->next)
      {
	add_actuator (l->data, node, copy);
      }

  if (copy)
    a = copy_actuator (a);
  else if (a->desc->flags & ACTUATOR_FLAG_CONTAINER)
    container_unlink_actuators (a);

  gtk_ctree_node_set_row_data_full (GTK_CTREE (actuator_tree), node, a,
				    ((GtkDestroyNotify) actuator_row_data_destroyed_cb));
}

static guchar
gdk_colour_to_paranormal_colour(gint16 colour)
{
  return (guchar) (colour / 255);
}

static gint16
paranormal_colour_to_gdk_colour(guchar colour)
{
  return (gint16) (colour * 255);
}

static void
int_changed_cb (GtkSpinButton *sb, int *i)
{
  *i = gtk_spin_button_get_value_as_int (sb);
}

static void
float_changed_cb (GtkSpinButton *sb, float *f)
{
  *f = gtk_spin_button_get_value_as_float (sb);
}

static void
string_changed_cb (GtkEditable *t, char **s)
{
  if (*s != gtk_object_get_data (GTK_OBJECT (t), "DEFAULT_OP_STRING"))
    g_free (*s);

  *s = gtk_editable_get_chars (t, 0, -1);
}

static void
color_changed_cb (GtkColorButton *cb, struct pn_color *c)
{
  GdkColor colour;

  gtk_color_button_get_color(cb, &colour);

  c->r = gdk_colour_to_paranormal_colour(colour.red);
  c->g = gdk_colour_to_paranormal_colour(colour.green);
  c->b = gdk_colour_to_paranormal_colour(colour.blue);
}

static void
boolean_changed_cb (GtkToggleButton *tb, gboolean *b)
{
  *b = gtk_toggle_button_get_active (tb);
}

static void
row_select_cb (GtkCTree *ctree, GtkCTreeNode *node,
	       gint column, gpointer data)
{
  struct pn_actuator *a;
  int opt_count = 0, i, j;
  GtkWidget *w;
  GtkObject *adj;

  a = (struct pn_actuator *)gtk_ctree_node_get_row_data (ctree, node);

  /* count the actuator's options (plus one) */
  if (a->desc->option_descs)
    while (a->desc->option_descs[opt_count++].name);
  else
    opt_count = 1;

  gtk_table_resize (GTK_TABLE (actuator_option_table), opt_count, 2);

  /* Actuator name */
  gtk_frame_set_label (GTK_FRAME (option_frame), a->desc->dispname);

  /* Actuator description */
  w = gtk_label_new (a->desc->doc);
  gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
  gtk_label_set_justify (GTK_LABEL (w), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (w), 0, .5);
  gtk_widget_show (w);
  gtk_table_attach (GTK_TABLE (actuator_option_table), w, 0, 2, 0, 1,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0,
		    3, 3);

  /* now add the options */
  for (i=1, j=0; i<opt_count; j++, i++)
    {
      w = gtk_label_new (a->desc->option_descs[j].name);
      gtk_widget_show (w);
      gtk_table_attach (GTK_TABLE (actuator_option_table), w,
			0, 1, i, i+1,
			GTK_SHRINK | GTK_FILL, 0,
			3, 3);
      switch (a->desc->option_descs[j].type)
	{
	case OPT_TYPE_INT:
	  adj = gtk_adjustment_new (a->options[j].val.ival,
				    G_MININT, G_MAXINT,
				    1, 2, 0);
	  w = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1.0, 0);
	  gtk_signal_connect (GTK_OBJECT (w), "changed",
			      GTK_SIGNAL_FUNC (int_changed_cb),
			      &a->options[j].val.ival);
	  break;
	case OPT_TYPE_FLOAT:
	  adj = gtk_adjustment_new (a->options[j].val.fval,
				    -G_MAXFLOAT, G_MAXFLOAT,
				    1, 2, 0);
	  w = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1.0, 5);
	  gtk_signal_connect (GTK_OBJECT (w), "changed",
			      GTK_SIGNAL_FUNC (float_changed_cb),
			      &a->options[j].val.fval);
	  break;
	case OPT_TYPE_STRING:
	  w = gtk_entry_new ();
	  gtk_widget_show (w);
	  gtk_entry_set_text (GTK_ENTRY (w), a->options[j].val.sval);
	  gtk_object_set_data (GTK_OBJECT (w), "DEFAULT_OP_STRING",
			       (gpointer)a->desc->option_descs[j].default_val.sval);
	  gtk_signal_connect (GTK_OBJECT (w), "changed",
			      GTK_SIGNAL_FUNC (string_changed_cb),
			      &a->options[j].val.sval);
	  break;
	case OPT_TYPE_COLOR:
	  {
	    /* FIXME: add some color preview */
            GdkColor *colour = g_new0(GdkColor, 1);

            colour->red = paranormal_colour_to_gdk_colour(a->options[j].val.cval.r);
            colour->green = paranormal_colour_to_gdk_colour(a->options[j].val.cval.g);
            colour->blue = paranormal_colour_to_gdk_colour(a->options[j].val.cval.b);

            w = gtk_color_button_new_with_color(colour);
	    g_signal_connect(G_OBJECT (w), "color-set",
			       G_CALLBACK (color_changed_cb),
			       &a->options[j].val.cval);
	    gtk_tooltips_set_tip (actuator_tooltips, GTK_WIDGET(w),
				  a->desc->option_descs[j].doc, NULL);
	  }
	  break;
	case OPT_TYPE_COLOR_INDEX:
	  adj = gtk_adjustment_new (a->options[j].val.ival,
				    0, 255,
				    1, 2, 0);
	  w = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1.0, 0);
	  gtk_signal_connect (GTK_OBJECT (w), "changed",
			      GTK_SIGNAL_FUNC (int_changed_cb),
			      &a->options[j].val.ival);
	  break;
	case OPT_TYPE_BOOLEAN:
	  w = gtk_check_button_new ();
	  gtk_widget_show (w);
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
					a->options[j].val.bval);
	  gtk_signal_connect (GTK_OBJECT (w), "clicked",
			      GTK_SIGNAL_FUNC (boolean_changed_cb),
			      &a->options[j].val.bval);
	  break;
	}
      gtk_widget_show (w);
      gtk_tooltips_set_tip (actuator_tooltips, w,
			    a->desc->option_descs[j].doc, NULL);
      gtk_table_attach (GTK_TABLE (actuator_option_table), w,
			1, 2, i, i+1,
			GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			0,
			3, 3);
    }

  gtk_widget_set_sensitive (actuator_remove_button, TRUE);
  gtk_widget_set_sensitive (actuator_add_button, a->desc->flags & ACTUATOR_FLAG_CONTAINER
			? TRUE : FALSE);

  selected_actuator_node = node;
}

static void
table_remove_all_cb (GtkWidget *widget, gpointer data)
{
  gtk_container_remove (GTK_CONTAINER (actuator_option_table),
			widget);
}

static void
row_unselect_cb (GtkCTree *ctree, GList *node, gint column,
		 gpointer user_data)
{
  gtk_frame_set_label (GTK_FRAME (option_frame), NULL);

  gtk_container_foreach (GTK_CONTAINER (actuator_option_table),
			 table_remove_all_cb, NULL);

  /* Can't remove something if nothing's selected */
  gtk_widget_set_sensitive (actuator_remove_button, FALSE);

  selected_actuator_node = NULL;
}

static void
add_actuator_cb (GtkButton *button, gpointer data)
{
  char *actuator_name;
  struct pn_actuator *a;

  gtk_label_get (GTK_LABEL (GTK_BIN (actuator_add_opmenu)->child),
                &actuator_name);

  a = create_actuator (actuator_name);
  g_assert (a);

  add_actuator (a, selected_actuator_node, FALSE);
}

static void
remove_actuator_cb (GtkButton *button, gpointer data)
{
  if (selected_actuator_node)
    gtk_ctree_remove_node (GTK_CTREE (actuator_tree),
			   selected_actuator_node);
}

/* Connect a node to its parent and replace the row data with
   a copy of the node */
static void
connect_actuators_cb (GtkCTree *ctree, GtkCTreeNode *node,
		      struct pn_actuator **root_ptr)
{
  struct pn_actuator *actuator, *parent, *copy;

  actuator = (struct pn_actuator *) gtk_ctree_node_get_row_data (ctree, node);
  if (GTK_CTREE_ROW (node)->parent)
    {
      /* Connect it to the parent */
      parent = (struct pn_actuator *)
	gtk_ctree_node_get_row_data (ctree, GTK_CTREE_ROW (node)->parent);
      container_add_actuator (parent, actuator);
    }
  else
    /* This is the root node; still gotta copy it, but we need to
       save the original to *root_ptr */
    *root_ptr = actuator;

  /* we don't want our copy getting destroyed */
  destroy_row_data = FALSE;

  copy = copy_actuator (actuator);
  gtk_ctree_node_set_row_data_full (ctree, node, copy,
				    ((GtkDestroyNotify)actuator_row_data_destroyed_cb));

  /* Ok, now you can destroy it */
  destroy_row_data = TRUE;
}

/* Extract (and connect) the actuators in the tree */
static struct pn_actuator *
extract_actuator (void)
{
  GtkCTreeNode *root, *selected;
  struct pn_actuator *root_actuator = NULL;

  root = gtk_ctree_node_nth (GTK_CTREE (actuator_tree), 0);
  if (root)
    gtk_ctree_post_recursive (GTK_CTREE (actuator_tree), root,
			      GTK_CTREE_FUNC (connect_actuators_cb),
			      &root_actuator);

  if (selected_actuator_node)
    {
      selected = selected_actuator_node;
      gtk_ctree_unselect (GTK_CTREE (actuator_tree), GTK_CTREE_NODE (selected));
      gtk_ctree_select (GTK_CTREE (actuator_tree), GTK_CTREE_NODE (selected));
    }

  return root_actuator;
}

/* If selector != NULL, then it's 'OK', otherwise it's 'Cancel' */
static void
load_sel_cb (GtkButton *button, GtkFileSelection *selector)
{
  if (selector)
    {
      static const char *fname;
      struct pn_actuator *a;
      GtkCTreeNode *root;
      mcs_handle_t *db;

      db = aud_cfg_db_open();
      fname = (char *) gtk_file_selection_get_filename (selector);
      a = load_preset (fname);
      aud_cfg_db_set_string(db, "paranormal", "last_path", (char*)fname);
      aud_cfg_db_close(db);
      if (! a)
	pn_error ("Unable to load file: \"%s\"", fname);
      else
	{
	  if ((root = gtk_ctree_node_nth (GTK_CTREE (actuator_tree), 0)))
	    gtk_ctree_remove_node (GTK_CTREE (actuator_tree), root);
	  add_actuator (a, NULL, FALSE);
	}
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
  if(aud_cfg_db_get_string(db, "paranormal", "last_path", &last_path)) {
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
save_sel_cb (GtkButton *button, GtkFileSelection *selector)
{
  if (selector)
    {
      const char *fname;
      struct pn_actuator *a;

      fname = (char *) gtk_file_selection_get_filename (selector);
      a = extract_actuator ();

      if (! save_preset (fname, a))
	pn_error ("unable to save preset to file: %s", fname);
    }

  gtk_widget_set_sensitive (cfg_dialog, TRUE);
}

static void
save_button_cb (GtkButton *button, gpointer data)
{
  GtkWidget *selector;

  selector = gtk_file_selection_new ("Save Preset");

  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (selector)->ok_button),
		      "clicked", GTK_SIGNAL_FUNC (save_sel_cb), selector);
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (selector)->cancel_button),
		      "clicked", GTK_SIGNAL_FUNC (save_sel_cb), NULL);

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
apply_settings (void)
{
  struct pn_rc rc;

  rc.actuator = extract_actuator ();

  pn_set_rc (&rc);
}

static void
apply_button_cb (GtkButton *button, gpointer data)
{
  apply_settings ();
}

static void
ok_button_cb (GtkButton *button, gpointer data)
{
  apply_settings ();
  gtk_widget_hide (cfg_dialog);
}

static void
cancel_button_cb (GtkButton *button, gpointer data)
{
  gtk_widget_destroy (cfg_dialog);
  cfg_dialog = NULL;
}

void
pn_configure (void)
{
  GtkWidget *notebook, *label, *scrollwindow, *menu, *menuitem;
  GtkWidget *paned, *vbox, *table, *bbox, *button;
  int i;


  if (! cfg_dialog)
    {
      /* The dialog */
      cfg_dialog = gtk_dialog_new ();
      gtk_window_set_title (GTK_WINDOW (cfg_dialog), "Paranormal Visualization Studio - Editor");
      gtk_widget_set_usize (cfg_dialog, 530, 370);
      gtk_container_border_width (GTK_CONTAINER (cfg_dialog), 8);
      gtk_signal_connect_object (GTK_OBJECT (cfg_dialog), "delete-event",
				 GTK_SIGNAL_FUNC (gtk_widget_hide),
				 GTK_OBJECT (cfg_dialog));

      /* The notebook */
      notebook = gtk_notebook_new ();
      gtk_widget_show (notebook);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (cfg_dialog)->vbox), notebook,
			  TRUE, TRUE, 0);

      /* Actuator page */
      paned = gtk_hpaned_new ();
      gtk_widget_show (paned);
      label = gtk_label_new ("Actuators");
      gtk_widget_show (label);
      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), paned, label);
      vbox = gtk_vbox_new (FALSE, 3);
      gtk_widget_show (vbox);
      gtk_paned_pack1 (GTK_PANED (paned), vbox, TRUE, FALSE);
      scrollwindow = gtk_scrolled_window_new (NULL, NULL);
      gtk_widget_show (scrollwindow);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollwindow),
				      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox), scrollwindow, TRUE, TRUE, 3);
      actuator_tree = gtk_ctree_new (1, 0);
      gtk_widget_show (actuator_tree);
      gtk_ctree_set_reorderable (GTK_CTREE (actuator_tree), TRUE);
      gtk_signal_connect (GTK_OBJECT (actuator_tree), "tree-select-row",
			  GTK_SIGNAL_FUNC (row_select_cb), NULL);
      gtk_signal_connect (GTK_OBJECT (actuator_tree), "tree-unselect-row",
			  GTK_SIGNAL_FUNC (row_unselect_cb), NULL);
      gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrollwindow),
					     actuator_tree);
      table = gtk_table_new (3, 2, TRUE);
      gtk_widget_show (table);
      gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 3);
      actuator_add_opmenu = gtk_option_menu_new ();
      gtk_widget_show (actuator_add_opmenu);
      menu = gtk_menu_new ();
      gtk_widget_show (menu);
      for (i=0; builtin_table[i]; i++)
	{
	  /* FIXME: Add actuator group support */
	  menuitem = gtk_menu_item_new_with_label (builtin_table[i]->dispname);
	  gtk_widget_show (menuitem);
	  gtk_menu_append (GTK_MENU (menu), menuitem);
	}
      gtk_option_menu_set_menu (GTK_OPTION_MENU (actuator_add_opmenu), menu);
      gtk_table_attach (GTK_TABLE (table), actuator_add_opmenu,
			0, 2, 0, 1,
			GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0,
			3, 3);
      actuator_add_button = gtk_button_new_from_stock(GTK_STOCK_ADD);
      gtk_widget_show (actuator_add_button);
      gtk_signal_connect (GTK_OBJECT (actuator_add_button), "clicked",
			  GTK_SIGNAL_FUNC (add_actuator_cb), NULL);
      gtk_table_attach (GTK_TABLE (table), actuator_add_button,
			0, 1, 1, 2,
			GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0,
			3, 3);
      actuator_remove_button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
      gtk_widget_set_sensitive (actuator_remove_button, FALSE);
      gtk_widget_show (actuator_remove_button);
      gtk_signal_connect (GTK_OBJECT (actuator_remove_button), "clicked",
			  GTK_SIGNAL_FUNC (remove_actuator_cb), NULL);
      gtk_table_attach (GTK_TABLE (table), actuator_remove_button,
			1, 2, 1, 2,
			GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0,
			3, 3);
      button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
      gtk_widget_show (button);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (load_button_cb), NULL);
      gtk_table_attach (GTK_TABLE (table), button,
			0, 1, 2, 3,
			GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0,
			3, 3);
      button = gtk_button_new_from_stock(GTK_STOCK_SAVE);
      gtk_widget_show (button);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (save_button_cb), NULL);
      gtk_table_attach (GTK_TABLE (table), button,
			1, 2, 2, 3,
			GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0,
			3, 3);

      /* Option table */
      option_frame = gtk_frame_new (NULL);
      gtk_widget_show (option_frame);
      gtk_container_set_border_width (GTK_CONTAINER (option_frame), 3);
      gtk_paned_pack2 (GTK_PANED (paned), option_frame, TRUE, TRUE);
      scrollwindow = gtk_scrolled_window_new (NULL, NULL);
      gtk_widget_show (scrollwindow);
      gtk_container_set_border_width (GTK_CONTAINER (scrollwindow), 3);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollwindow),
				      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
      gtk_container_add (GTK_CONTAINER (option_frame), scrollwindow);
      actuator_option_table = gtk_table_new (0, 2, FALSE);
      gtk_widget_show (actuator_option_table);
      gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrollwindow),
					     actuator_option_table);
      gtk_paned_set_position (GTK_PANED (paned), 0);
      actuator_tooltips = gtk_tooltips_new ();
      gtk_tooltips_enable (actuator_tooltips);

      /* Build the initial actuator actuator_tree */
      if (pn_rc->actuator)
	{
	  add_actuator (pn_rc->actuator, NULL, TRUE);
	  gtk_widget_set_sensitive (actuator_add_button, FALSE);
	}

      /* OK / Apply / Cancel */
      bbox = gtk_hbutton_box_new ();
      gtk_widget_show (bbox);
      gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
      gtk_button_box_set_spacing (GTK_BUTTON_BOX (bbox), 8);
      gtk_button_box_set_child_size (GTK_BUTTON_BOX (bbox), 64, 0);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (cfg_dialog)->action_area),
			  bbox, FALSE, FALSE, 0);

      button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
      gtk_widget_show (button);
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NORMAL);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (cancel_button_cb), NULL);
      gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

      button = gtk_button_new_from_stock (GTK_STOCK_APPLY);
      gtk_widget_show (button);
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NORMAL);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (apply_button_cb), NULL);
      gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

      button = gtk_button_new_from_stock (GTK_STOCK_OK);
      gtk_widget_show (button);
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NORMAL);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (ok_button_cb), NULL);
      gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
    }

  gtk_widget_show (cfg_dialog);
  gtk_widget_grab_focus (cfg_dialog);
}
