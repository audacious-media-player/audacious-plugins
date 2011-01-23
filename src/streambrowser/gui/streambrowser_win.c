
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "../streambrowser.h"
#include "streambrowser_win.h"
#include "../bookmarks.h"


typedef struct {

	streamdir_t*	streamdir;
	GtkWidget*		table;
	GtkWidget*		tree_view;

} streamdir_gui_t;


void					(* update_function) (streamdir_t *streamdir, category_t *category, streaminfo_t *streaminfo, gboolean add_to_playlist);

static GtkWidget*		gtk_label_new_with_icon(gchar *icon_filename, gchar *label_text);
static GtkWidget*		gtk_streamdir_tree_view_new();
static GtkWidget*		gtk_streamdir_table_new(GtkWidget *tree_view);

static gboolean			on_notebook_switch_page(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, gpointer data);
static gboolean			on_tree_view_cursor_changed(GtkTreeView *tree_view, gpointer data);
static gboolean			on_add_button_clicked(GtkButton *button, gpointer data);
static gboolean			on_bookmark_button_clicked(GtkButton *button, gpointer data);
static gboolean			on_search_entry_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);
static gboolean			on_tree_view_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);
static gboolean			on_tree_view_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer data);

static streamdir_gui_t*	find_streamdir_gui_by_name(gchar *name);
static streamdir_gui_t*	find_streamdir_gui_by_table(GtkTable *table);
static streamdir_gui_t*	find_streamdir_gui_by_streamdir(streamdir_t *streamdir);
static gboolean			tree_view_search_equal_func(GtkTreeModel *model, gint column, const gchar *key, GtkTreeIter *iter, gpointer data);

GtkWidget*		notebook;
static GtkWidget*		search_entry;
static GtkWidget*		add_button;
static GtkWidget*		bookmark_button;
static GList*			streamdir_gui_list = NULL;
static GtkCellRenderer*	cell_renderer_pixbuf;
static GtkCellRenderer*	cell_renderer_text;

static gboolean			tree_view_button_pressed = FALSE;

static void list_cleanup (void)
{
	GList * node;
	for (node = streamdir_gui_list; node; node = node->next)
	{
		streamdir_gui_t * gui = node->data;
		streamdir_delete (gui->streamdir);
		memset (gui, 0, sizeof gui);
		g_free (gui);
	}

	g_list_free (streamdir_gui_list);
	streamdir_gui_list = NULL;
}

GtkWidget *streambrowser_win_init()
{
	/* notebook */
	notebook = gtk_notebook_new();
	g_signal_connect(G_OBJECT(notebook), "switch-page", G_CALLBACK(on_notebook_switch_page), NULL);
	gtk_widget_show_all(notebook);

	GtkWidget *search_label = gtk_label_new(_("Search:"));
	gtk_widget_show(search_label);

	/* search entry */
	search_entry = gtk_entry_new();
	g_signal_connect(G_OBJECT(search_entry), "key-press-event", G_CALLBACK(on_search_entry_key_pressed), NULL);
	gtk_widget_show(search_entry);

	GtkWidget *hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), search_label, FALSE, TRUE, 3);
	gtk_box_pack_start(GTK_BOX(hbox1), search_entry, TRUE, TRUE, 3);
	gtk_widget_show(hbox1);

	/* add button */
	add_button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(on_add_button_clicked), NULL);
	gtk_widget_show(add_button);

	/* bookmark button */
	bookmark_button = gtk_button_new_with_label(_("Add Bookmark"));
/*	gtk_button_set_image(GTK_BUTTON(bookmark_button), gtk_image_new_from_file(BOOKMARKS_ICON)); */
	g_signal_connect(G_OBJECT(bookmark_button), "clicked", G_CALLBACK(on_bookmark_button_clicked), NULL);
	gtk_widget_show(bookmark_button);

	GtkWidget *vbox1 = gtk_vbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox1), notebook, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), add_button, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), bookmark_button, FALSE, TRUE, 0);
	gtk_widget_show(vbox1);

	/* others */
	cell_renderer_pixbuf = gtk_cell_renderer_pixbuf_new();
	cell_renderer_text = gtk_cell_renderer_text_new();

	g_signal_connect (vbox1, "destroy", (GCallback) list_cleanup, NULL);
	return vbox1;
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

		gtk_widget_show_all(notebook);
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
		gtk_tree_store_set(store, &iter, 0, "gtk-directory", 1, category->name, 2, "", 3, PANGO_WEIGHT_NORMAL, -1);
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
		gtk_tree_store_set(store, &new_iter, 0, "gtk-media-play", 1, streaminfo->name, 2, streaminfo->current_track, 3, PANGO_WEIGHT_NORMAL, -1);
	}

	gtk_tree_path_free(path);
}

void streambrowser_win_set_streaminfo(streamdir_t *streamdir, category_t *category, streaminfo_t *streaminfo)
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

	/* find the corresponding streaminfo tree iter */
	path = gtk_tree_path_new_from_indices(category_get_index(streamdir, category), streaminfo_get_index(category, streaminfo), -1);
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path))
		return;

	/* update the streaminfo*/
	gtk_tree_store_set(store, &new_iter, 0, "gtk-media-play", 1, streaminfo->name, 2, streaminfo->current_track, 3, PANGO_WEIGHT_NORMAL -1);

	gtk_tree_path_free(path);
}

void streambrowser_win_set_update_function(void (*function) (streamdir_t *streamdir, category_t *category, streaminfo_t *streaminfo, gboolean add_to_playlist))
{
	update_function = function;
}

void streambrowser_win_set_category_state(streamdir_t *streamdir, category_t *category, gboolean fetching)
{
	streamdir_gui_t *streamdir_gui = find_streamdir_gui_by_streamdir(streamdir);
	if (streamdir_gui == NULL) {
		failure("gui: streambrowser_win_set_category() called with non-existent streamdir\n");
		return;
	}

	GtkTreeView *tree_view = GTK_TREE_VIEW(streamdir_gui->tree_view);
	GtkTreeStore *store = GTK_TREE_STORE(gtk_tree_view_get_model(tree_view));
	GtkTreePath *path;
	GtkTreeIter iter;

	/* find the corresponding category tree iter */
	path = gtk_tree_path_new_from_indices(category_get_index(streamdir, category), -1);
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path))
		return;

	if (fetching) {
		gtk_tree_store_set(store, &iter, 0, "gtk-refresh", 1, category->name, 2, "", 3, PANGO_WEIGHT_BOLD, -1);
	}
	else {
		gtk_tree_store_set(store, &iter, 0, "gtk-directory", 1, category->name, 2, "", 3, PANGO_WEIGHT_NORMAL, -1);

		/* we also expand the corresponding category tree element in the treeview */
		gtk_tree_view_expand_row(tree_view, path, FALSE);
	}
}

void streambrowser_win_set_streaminfo_state(streamdir_t *streamdir, category_t *category, streaminfo_t *streaminfo, gboolean fetching)
{
	streamdir_gui_t *streamdir_gui = find_streamdir_gui_by_streamdir(streamdir);
	GtkTreeView *tree_view = GTK_TREE_VIEW(streamdir_gui->tree_view);
	GtkTreeStore *store = GTK_TREE_STORE(gtk_tree_view_get_model(tree_view));
	GtkTreePath *path;
	GtkTreeIter iter;

	/* find the corresponding category tree iter */
	path = gtk_tree_path_new_from_indices(category_get_index(streamdir, category), streaminfo_get_index(category, streaminfo), -1);
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path))
		return;

	if (fetching) {
		gtk_tree_store_set(store, &iter, 0, "gtk-media-play", 1, streaminfo->name, 2, streaminfo->current_track, 3, PANGO_WEIGHT_BOLD, -1);
	}
	else {
		gtk_tree_store_set(store, &iter, 0, "gtk-media-play", 1, streaminfo->name, 2, streaminfo->current_track, 3, PANGO_WEIGHT_NORMAL, -1);
	}
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

	GtkTreeStore *store = gtk_tree_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), GTK_TREE_MODEL(store));

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), TRUE);
	gtk_tree_view_set_search_entry(GTK_TREE_VIEW(tree_view), GTK_ENTRY(search_entry));
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(tree_view), tree_view_search_equal_func, NULL, NULL);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(tree_view), 1);
	g_signal_connect(G_OBJECT(tree_view), "key-press-event", G_CALLBACK(on_tree_view_key_pressed), NULL);
	g_signal_connect(G_OBJECT(tree_view), "cursor-changed", G_CALLBACK(on_tree_view_cursor_changed), NULL);
	g_signal_connect(G_OBJECT(tree_view), "button-press-event", G_CALLBACK(on_tree_view_button_pressed), NULL);

	GtkTreeViewColumn *column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column, cell_renderer_pixbuf, TRUE);
	gtk_tree_view_column_add_attribute(column, cell_renderer_pixbuf, "stock-id", 0);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column, cell_renderer_text, TRUE);
	gtk_tree_view_column_add_attribute(column, cell_renderer_text, "text", 1);
	gtk_tree_view_column_add_attribute(column, cell_renderer_text, "weight", 3);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_title(column, _("Stream name"));
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column, cell_renderer_text, TRUE);
	gtk_tree_view_column_add_attribute(column, cell_renderer_text, "text", 2);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_title(column, _("Now playing"));
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

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

	/* only enable searching in the current tree (tab) */

	streamdir_gui_t *streamdir_gui;
	int i;

	for (i = 0; i < g_list_length(streamdir_gui_list); i++) {
		streamdir_gui = g_list_nth_data(streamdir_gui_list, i);

		if (i == page_num)
			gtk_tree_view_set_search_column(GTK_TREE_VIEW(streamdir_gui->tree_view), 1);
		else
			gtk_tree_view_set_search_column(GTK_TREE_VIEW(streamdir_gui->tree_view), -1);
	}

	/* clear the search box */
	gtk_entry_set_text(GTK_ENTRY(search_entry), "");

	/* if this is the last page, aka the bookmarks page, bookmark button has to say "Remove bookmark", instead of "Add bookmark"0 */
	if (page_num == gtk_notebook_get_n_pages(notebook) - 1) {
		gtk_button_set_label(GTK_BUTTON(bookmark_button), _("Remove Bookmark"));
	}
	else {
		gtk_button_set_label(GTK_BUTTON(bookmark_button), _("Add Bookmark"));
	}

	return TRUE;
}


static gboolean on_tree_view_cursor_changed(GtkTreeView *tree_view, gpointer data)
{
	/* only do the refresh if this cursor change occured due to a mouse click */
	if (!tree_view_button_pressed)
		return FALSE;

	tree_view_button_pressed = FALSE;

	GtkTreePath *path;
	GtkTreeViewColumn *focus_column;

	/* get the currently selected tree view */
	GtkWidget *table = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
	streamdir_gui_t *streamdir_gui = find_streamdir_gui_by_table(GTK_TABLE(table));
	if (streamdir_gui == NULL)
		return FALSE;

	/* get the currently selected path in the tree */
	gtk_tree_view_get_cursor(tree_view, &path, &focus_column);

	if (path == NULL || gtk_tree_path_get_depth(path) == 0)
		return FALSE;

	gint *indices = gtk_tree_path_get_indices(path);
	int category_index = indices[0];
	streamdir_t *streamdir = streamdir_gui->streamdir;
	category_t *category = category_get_by_index(streamdir_gui->streamdir, category_index);

	/* if the selected item is a category */
	if (gtk_tree_path_get_depth(path) == 1) {
		if (!gtk_tree_view_row_expanded(tree_view, path)) {
			gtk_entry_set_text(GTK_ENTRY(search_entry), "");
			update_function(streamdir, category, NULL, FALSE);
		}

		gtk_tree_path_free(path);
	}
	/* if the selected item is a streaminfo */
	else {
		int streaminfo_index = indices[1];

		gtk_tree_path_free(path);

		/* get the currently selected stream (info) */
		streaminfo_t *streaminfo = streaminfo_get_by_index(category, streaminfo_index);

		gtk_entry_set_text(GTK_ENTRY(search_entry), "");
		update_function(streamdir, category, streaminfo, FALSE);
	}

	return FALSE;
}

static gboolean on_tree_view_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	/* double click adds the currently selected stream to the playlist */
	if (event->type == GDK_2BUTTON_PRESS) {
		tree_view_button_pressed = FALSE;
		on_add_button_clicked(NULL, NULL);
	}
	/* single click triggers a refresh of the selected item */
	else {
		tree_view_button_pressed = TRUE;
	}

	return FALSE;
}

static gboolean on_add_button_clicked(GtkButton *button, gpointer data)
{
	GtkTreePath *path;
	GtkTreeViewColumn *focus_column;

	/* get the currently selected tree view */
	GtkWidget *table = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
	streamdir_gui_t *streamdir_gui = find_streamdir_gui_by_table(GTK_TABLE(table));
	if (streamdir_gui == NULL)
		return TRUE;

	GtkTreeView *tree_view = GTK_TREE_VIEW(streamdir_gui->tree_view);

	/* get the currently selected path in the tree */
	gtk_tree_view_get_cursor(tree_view, &path, &focus_column);

	if (path == NULL)
		return TRUE;

	gint *indices = gtk_tree_path_get_indices(path);
	if (gtk_tree_path_get_depth(path) == 1) {
		if (gtk_tree_view_row_expanded(tree_view, path))
			gtk_tree_view_collapse_row(tree_view, path);
		else
			gtk_tree_view_expand_row(tree_view, path, FALSE);

		gtk_tree_path_free(path);
		return TRUE;
	}

	int category_index = indices[0];
	int streaminfo_index = indices[1];

	gtk_tree_path_free(path);

	/* get the currently selected stream (info) */
	streamdir_t *streamdir = streamdir_gui->streamdir;
	category_t *category = category_get_by_index(streamdir_gui->streamdir, category_index);
	streaminfo_t *streaminfo = streaminfo_get_by_index(category, streaminfo_index);

	gtk_entry_set_text(GTK_ENTRY(search_entry), "");
	update_function(streamdir, category, streaminfo, TRUE);

	return TRUE;
}

static gboolean on_bookmark_button_clicked(GtkButton *button, gpointer data)
{
	GtkTreePath *path;
	GtkTreeViewColumn *focus_column;

	/* get the currently selected tree view */
	GtkWidget *table = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
	streamdir_gui_t *streamdir_gui = find_streamdir_gui_by_table(GTK_TABLE(table));
	if (streamdir_gui == NULL)
		return TRUE;

	GtkTreeView *tree_view = GTK_TREE_VIEW(streamdir_gui->tree_view);

	/* get the currently selected path in the tree */
	gtk_tree_view_get_cursor(tree_view, &path, &focus_column);

	if (path == NULL)
		return TRUE;

	gint *indices = gtk_tree_path_get_indices(path);
	if (gtk_tree_path_get_depth(path) == 1) {
		gtk_tree_path_free(path);
		return TRUE;
	}

	int category_index = indices[0];
	int streaminfo_index = indices[1];

	gtk_tree_path_free(path);

	/* get the currently selected stream (info) */
	streamdir_t *streamdir = streamdir_gui->streamdir;
	category_t *category = category_get_by_index(streamdir_gui->streamdir, category_index);
	streaminfo_t *streaminfo = streaminfo_get_by_index(category, streaminfo_index);

	/* if we have the bookmarks tab focused, we remove the stream, rather than add it. otherwise we simply add it */
	if (strcmp(streamdir->name, BOOKMARKS_NAME) == 0) {
		bookmark_remove(streaminfo->name);

		update_function(streamdir, category, NULL, FALSE);
	}
	else {
		bookmark_t bookmark;
		strncpy(bookmark.streamdir_name, streamdir->name, DEF_STRING_LEN);
		strncpy(bookmark.name, streaminfo->name, DEF_STRING_LEN);
		strncpy(bookmark.playlist_url, streaminfo->playlist_url, DEF_STRING_LEN);
		strncpy(bookmark.url, streaminfo->url, DEF_STRING_LEN);

		bookmark_add(&bookmark);

		/* find the corresponding category (having the current streamdir name) in the bookmarks */
		streamdir_t *bookmarks_streamdir = find_streamdir_gui_by_name(BOOKMARKS_NAME)->streamdir;
		category_t *bookmarks_category = category_get_by_name(bookmarks_streamdir, streamdir->name);

		update_function(bookmarks_streamdir, bookmarks_category, NULL, FALSE);
	}

	return TRUE;
}

static gboolean on_search_entry_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Return || event->keyval == GDK_KP_Enter) {
		on_add_button_clicked(GTK_BUTTON(add_button), NULL);
		return TRUE;
	}

	if (event->keyval == GDK_Escape) {
		gtk_entry_set_text(GTK_ENTRY(search_entry), "");
		return FALSE;
	}

	return FALSE;
}

static gboolean on_tree_view_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Down || event->keyval == GDK_Up)
		return FALSE;
	else
		if (event->keyval == GDK_Return || event->keyval == GDK_KP_Enter) {
			on_add_button_clicked(GTK_BUTTON(add_button), NULL);
			return FALSE;
		}

	gtk_widget_grab_focus(search_entry);
	on_search_entry_key_pressed(widget, event, data);

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

static streamdir_gui_t* find_streamdir_gui_by_streamdir(streamdir_t *streamdir)
{
	GList *iterator;
	streamdir_gui_t *streamdir_gui;

	for (iterator = g_list_first(streamdir_gui_list); iterator != NULL; iterator = g_list_next(iterator)) {
		streamdir_gui = iterator->data;

		if ((void *) streamdir_gui->streamdir == (void *) streamdir)
			return streamdir_gui;
	}

	return NULL;
}

static gboolean tree_view_search_equal_func(GtkTreeModel *model, gint column, const gchar *key, GtkTreeIter *iter, gpointer data)
{
	if (column == -1)
		return TRUE;

	GValue value = {0, };
	gboolean ret;

	gtk_tree_model_get_value(model, iter, column, &value);
	const gchar *string = g_value_get_string(&value);

	if (string == NULL || string[0] == '\0' || key == NULL || key[0] == '\0')
		ret = TRUE;

	ret = !mystrcasestr(string, key);

	g_value_unset(&value);

	return ret;
}

