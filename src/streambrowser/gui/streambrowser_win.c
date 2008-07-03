
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "../streambrowser.h"
#include "streambrowser_win.h"


typedef struct {

	streamdir_t*		streamdir;
	GtkWidget*		table;
	GtkWidget*		tree_view;

} streamdir_gui_t;


void				(* update_function) (streamdir_t *streamdir, category_t *category, streaminfo_t *streaminfo);

static GtkWidget*		gtk_label_new_with_icon(gchar *icon_filename, gchar *label_text);
static GtkWidget*		gtk_streamdir_tree_view_new();
static GtkWidget*		gtk_streamdir_table_new(GtkWidget *tree_view);

static gboolean			on_notebook_switch_page(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, gpointer data);
static gboolean			on_tree_view_cursor_changed(GtkTreeView *tree_view, gpointer data);
static gboolean			on_add_button_clicked(GtkButton *button, gpointer data);

static streamdir_gui_t*		find_streamdir_gui_by_name(gchar *name);
static streamdir_gui_t*		find_streamdir_gui_by_tree_view(GtkTreeView *tree_view);
static streamdir_gui_t*		find_streamdir_gui_by_table(GtkTable *table);


static GtkWidget*		notebook;
static GtkWidget*		search_entry;
static GtkWidget*		add_button;
static GtkWidget*		streambrowser_window;
static GList*			streamdir_gui_list;
static GtkCellRenderer*		cell_renderer_pixbuf;
static GtkCellRenderer*		cell_renderer_text;


void streambrowser_win_init()
{
	/* notebook */
	notebook = gtk_notebook_new();
	g_signal_connect(G_OBJECT(notebook), "switch-page", G_CALLBACK(on_notebook_switch_page), NULL);
	gtk_widget_show(notebook);

	GtkWidget *search_label = gtk_label_new(_("Search:"));
	gtk_widget_show(search_label);

	/* search entry */
	search_entry = gtk_entry_new();
	gtk_widget_show(search_entry);
	
	GtkWidget *hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), search_label, FALSE, TRUE, 3);
	gtk_box_pack_start(GTK_BOX(hbox1), search_entry, TRUE, TRUE, 3);
	gtk_widget_show(hbox1);

	/* add button */
	add_button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(on_add_button_clicked), NULL);
	gtk_widget_show(add_button);

	GtkWidget *vbox1 = gtk_vbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox1), notebook, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), add_button, FALSE, TRUE, 0);
	gtk_widget_show(vbox1);

	/* streambrowser window */
	streambrowser_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(streambrowser_window), _("Stream browser"));
	gtk_window_set_position(GTK_WINDOW(streambrowser_window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(streambrowser_window), 700, 400);
	g_signal_connect(G_OBJECT(streambrowser_window), "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), streambrowser_window);
	gtk_container_add(GTK_CONTAINER(streambrowser_window), vbox1);

	/* others */
	cell_renderer_pixbuf = gtk_cell_renderer_pixbuf_new();
	g_object_set(G_OBJECT(cell_renderer_pixbuf), "stock-id", "gtk-directory", NULL);
	cell_renderer_text = gtk_cell_renderer_text_new();
}

void streambrowser_win_done()
{
}

void streambrowser_win_show()
{
	gtk_widget_show(streambrowser_window);
}

void streambrowser_win_hide()
{
	gtk_widget_hide(streambrowser_window);
}

void streambrowser_win_set_streamdir(streamdir_t *streamdir, gchar *icon_filename)
{
	GtkWidget *tree_view = NULL;
	
	/* search for an old instance of this streamdir and replace it */
	streamdir_gui_t *streamdir_gui = find_streamdir_gui_by_name(streamdir->name);
	if (streamdir_gui != NULL) {
		streamdir_delete(streamdir_gui->streamdir);
		streamdir_gui->streamdir = streamdir;
		tree_view = streamdir_gui->tree_view;
	}
	/* if no older instance of this streamdir has been found, we add a brand new one */
	else {
		streamdir_gui = g_malloc(sizeof(streamdir_gui_t));

		tree_view = gtk_streamdir_tree_view_new();

		GtkWidget *table = gtk_streamdir_table_new(tree_view);
		gtk_widget_show_all(table);

		GtkWidget *label = gtk_label_new_with_icon(icon_filename, streamdir->name);
		gtk_widget_show_all(label);

		streamdir_gui->streamdir = streamdir;
		streamdir_gui->tree_view = tree_view;
		streamdir_gui->table = table;

		streamdir_gui_list = g_list_append(streamdir_gui_list, streamdir_gui);
		
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table, label);
	}

	/* fill the tree with categories */
	GtkTreeIter iter;
	GtkTreeStore *store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view)));
	
	gtk_tree_store_clear(store);

	int i, count = category_get_count(streamdir);
	category_t *category;
	for (i = 0; i < count; i++) {
		category = category_get_by_index(streamdir, i);

		gtk_tree_store_append(store, &iter, NULL);
		gtk_tree_store_set(store, &iter, 0, NULL, 1, category->name, 2, "", -1);
	}
}

void streambrowser_win_set_category(streamdir_t *streamdir, category_t *category)
{
	streamdir_gui_t *streamdir_gui = find_streamdir_gui_by_name(streamdir->name);
	if (streamdir_gui == NULL) {
		failure("gui: streambrowser_win_set_category() called with non-existent streamdir\n");
		return;
	}
	
	GtkTreeView *tree_view = GTK_TREE_VIEW(streamdir_gui->tree_view);
	GtkTreeStore *store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view)));
	GtkTreePath *path;
	GtkTreeIter iter, new_iter;
	
	/* clear all the previously added streaminfo in this category */
	path = gtk_tree_path_new_from_indices(category_get_index(streamdir, category), 0, -1);
	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path)) {
		while (gtk_tree_store_remove(store, &iter))
			;
	}
	
	/* find the corresponding category tree iter */
	path = gtk_tree_path_new_from_indices(category_get_index(streamdir, category), -1);
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path))
		return;
	
	/* append the new streaminfos to the current category / iter */
	int i, count = streaminfo_get_count(category);
	streaminfo_t *streaminfo;
	for (i = 0; i < count; i++) {
		streaminfo = streaminfo_get_by_index(category, i);

		gtk_tree_store_append(store, &new_iter, &iter);
		gtk_tree_store_set(store, &new_iter, 0, NULL, 1, streaminfo->name, 2, streaminfo->current_track, -1);
	}
}

void streambrowser_win_set_update_function(void (*function) (streamdir_t *streamdir, category_t *category, streaminfo_t *streaminfo))
{
	update_function = function;
}

static GtkWidget* gtk_label_new_with_icon(gchar *icon_filename, gchar *label_text)
{
	GtkWidget *hbox = gtk_hbox_new(FALSE, 1);
	GtkWidget *label = gtk_label_new(label_text);
	GtkWidget *icon = gtk_image_new_from_file(icon_filename);

	gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	
	return hbox;
}

static GtkWidget *gtk_streamdir_tree_view_new()
{
	GtkWidget *tree_view = gtk_tree_view_new();

	GtkTreeStore *store = gtk_tree_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), GTK_TREE_MODEL(store));

	// todo: why doesn't the tree view allow to be resized?
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), TRUE);
	gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tree_view), TRUE);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(tree_view), TRUE);
	gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(tree_view), FALSE);

	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "", cell_renderer_pixbuf, "pixbuf", 0, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, _("Stream name"), cell_renderer_text, "text", 1, NULL);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, _("Now playing"), cell_renderer_text, "text", 2, NULL);

	g_signal_connect(G_OBJECT(tree_view), "cursor-changed", G_CALLBACK(on_tree_view_cursor_changed), NULL);

	return tree_view;
}

static GtkWidget* gtk_streamdir_table_new(GtkWidget *tree_view)
{
	GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scrolled_window), tree_view);
	
	GtkWidget *table = gtk_table_new(1, 1, FALSE);
	gtk_table_attach(GTK_TABLE(table), scrolled_window, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	return table;
}

static gboolean on_notebook_switch_page(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, gpointer data)
{
	if (page_num < 0)
		return FALSE;

	/* update the current selected stream */

	/*
	streamdir_gui_t *streamdir_gui = g_list_nth_data(streamdir_gui_list, page_num);
	update_function(streamdir_gui->streamdir, NULL, NULL);
	*/

	/* clear the search box */
	gtk_entry_set_text(GTK_ENTRY(search_entry), "");

	return TRUE;
}

static gboolean on_tree_view_cursor_changed(GtkTreeView *tree_view, gpointer data)
{
	GtkTreePath *path;
	GtkTreeViewColumn *focus_column;

	/* obtain the current category */
	gtk_tree_view_get_cursor(tree_view, &path, &focus_column);
	
	if (path == NULL)
		return TRUE;
	
	gint *indices = gtk_tree_path_get_indices(path);
	if (gtk_tree_path_get_depth(path) != 1) {
		gtk_tree_path_free(path);
		return TRUE;
	}
	
	int category_index = indices[0];
	
	gtk_tree_path_free(path);
	
	streamdir_gui_t *streamdir_gui = find_streamdir_gui_by_tree_view(tree_view);
	if (streamdir_gui == NULL)
		return TRUE;
	
	/* issue an update on the current category */	
	update_function(streamdir_gui->streamdir, category_get_by_index(streamdir_gui->streamdir, category_index), NULL);
	
	/* clear the search box */
	gtk_entry_set_text(GTK_ENTRY(search_entry), "");

	return TRUE;
}

static gboolean on_add_button_clicked(GtkButton *button, gpointer data)
{
	GtkTreePath *path;
	GtkTreeViewColumn *focus_column;

	GtkWidget *table = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
	streamdir_gui_t *streamdir_gui = find_streamdir_gui_by_table(GTK_TABLE(table));
	if (streamdir_gui == NULL)
		return TRUE;

	GtkWidget *tree_view = streamdir_gui->tree_view;
	
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(tree_view), &path, &focus_column);
	
	if (path == NULL)
		return TRUE;

	gint *indices = gtk_tree_path_get_indices(path);
	if (gtk_tree_path_get_depth(path) != 2) {
		gtk_tree_path_free(path);
		return TRUE;
	}
	
	int category_index = indices[0];
	int streaminfo_index = indices[1];
	
	gtk_tree_path_free(path);
	
	streamdir_t *streamdir = streamdir_gui->streamdir;
	category_t *category = category_get_by_index(streamdir_gui->streamdir, category_index);
	streaminfo_t *streaminfo = streaminfo_get_by_index(category, streaminfo_index);
	
	update_function(streamdir, category, streaminfo);

	return TRUE;
}

static streamdir_gui_t *find_streamdir_gui_by_name(gchar *name)
{
	GList *iterator;
	streamdir_gui_t *streamdir_gui;

	for (iterator = g_list_first(streamdir_gui_list); iterator != NULL; iterator = g_list_next(iterator)) {
		streamdir_gui = iterator->data;

		if (strcmp(streamdir_gui->streamdir->name, name) == 0)
			return streamdir_gui;
	}
	
	return NULL;
}

static streamdir_gui_t *find_streamdir_gui_by_tree_view(GtkTreeView *tree_view)
{
	GList *iterator;
	streamdir_gui_t *streamdir_gui;

	for (iterator = g_list_first(streamdir_gui_list); iterator != NULL; iterator = g_list_next(iterator)) {
		streamdir_gui = iterator->data;

		if ((void *) streamdir_gui->tree_view == (void *) tree_view)
			return streamdir_gui;
	}
	
	return NULL;
}

static streamdir_gui_t *find_streamdir_gui_by_table(GtkTable *table)
{
	GList *iterator;
	streamdir_gui_t *streamdir_gui;

	for (iterator = g_list_first(streamdir_gui_list); iterator != NULL; iterator = g_list_next(iterator)) {
		streamdir_gui = iterator->data;

		if ((void *) streamdir_gui->table == (void *) table)
			return streamdir_gui;
	}
	
	return NULL;
}

